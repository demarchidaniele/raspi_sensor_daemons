#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "shmem.h"
#include "daemon.h"

#define SHMEM_KEY 3935
#define LOG_STR_LEN 128
#define MAX_LOG_EVENTS 255 
#define MAX_LIGHT_EVENTS 128
#define LOG(...)	{char log_tmp_string[256]; 																		\
				sprintf(log_tmp_string, __VA_ARGS__);															\
				gettimeofday(&(as3935->log[as3935->log_pointer].timestamp), NULL);								\
				int len = strlen(log_tmp_string) + strlen(as3935->log[as3935->log_pointer].string);						\
				strncat(as3935->log[as3935->log_pointer].string, 													\
					log_tmp_string,																			\
					MIN(len, LOG_STR_LEN - 1) 																\
					);																						\
				if (log_tmp_string[strlen(log_tmp_string) - 1] == '\n') {												\
					as3935->log[as3935->log_pointer].string[strlen(as3935->log[as3935->log_pointer].string) - 1] = 0x00;\
					as3935->log_pointer++;																	\
					as3935->log_counter++;																	\
					if(as3935->log_pointer >= MAX_LOG_EVENTS) as3935->log_pointer= 0;							\
					if(as3935->log_counter > MAX_LOG_EVENTS) as3935->log_counter= MAX_LOG_EVENTS;			\
					memset(as3935->log[as3935->log_pointer].string, 0x00, LOG_STR_LEN);							\
				}																							\
				}

#define GPIO_IRQ_PIN RPI_V2_GPIO_P1_22

uint32_t opt_wait_seconds = 0;
uint8_t opt_verbose = 0;
uint8_t opt_daemonize = 0;
uint8_t opt_query_daemon = 0;
uint8_t opt_daemonkill = 0;
uint8_t opt_daemonlog = 0;
uint8_t opt_print_json = 0;
uint8_t opt_query_command = 0;

#define OUTDOOR 0
#define INDOOR 1
static int NFL_uVrms[2][8]={{390, 630, 860, 1100, 1140, 1570, 1800, 2000},  //outdoor
							  {28, 45, 62, 78, 95, 112, 130, 146}}; //indoor
static unsigned char minLightnig[4] = {1, 5, 9, 16};

#define KM_OUT_OF_RANGE 0x3F
#define KM_OVERHEAD 0x3F
#define ANT_TUNING_DIV_RATIO_16    0x00
#define ANT_TUNING_DIV_RATIO_32    0x01
#define ANT_TUNING_DIV_RATIO_64    0x02
#define ANT_TUNING_DIV_RATIO_128  0x03

struct as3935_t{
	uint8_t indoor_settings;

	uint8_t noise_floor_level;
	uint8_t cmd_set_noise_floor_level;
	uint8_t cmd_tune_antenna;

	uint8_t watchdog_threshold;
	uint8_t min_num_lightnings;
	uint8_t spike_rejection;
	uint8_t mask_disturber;
	uint8_t ant_tuner_capacitor;

	float ant_resonance[0x0F];

	struct lightning_events_t{
		struct timeval timestamp;
		uint32_t energy;
		uint8_t distance;
	} lightning_events[MAX_LIGHT_EVENTS];
	uint8_t lightning_events_counter;
	uint8_t lightning_events_pointer;

	struct log_t{
		struct timeval timestamp;
		char string[LOG_STR_LEN];
	} log[MAX_LOG_EVENTS];
	uint8_t log_counter;
	uint8_t log_pointer;

	uint8_t reg[0x33];
} *as3935;


uint8_t as3935_getNFLEV();

uint8_t as3935_read(uint8_t reg)
{
	DBGLN("as3935_read [0x%02x]: ", reg);
	uint8_t buf_tx[2], buf_rx[2];
	buf_tx[0] = 0x40 | (reg & 0x3F);
	buf_tx[1] = 0;

	buf_rx[0] = 0;
	buf_rx[1] = 0;

	bcm2835_spi_transfernb(buf_tx, buf_rx, 2);
	DBG("%02x\n", buf_rx[1]);

	return buf_rx[1];
}

uint8_t as3935_write(uint8_t reg, uint8_t val)
{
	DBGLN("as3935_write [0x%02x]: 0x%02x\n", reg, val);
	uint8_t buf[2];
	buf[0] = reg & 0x3F;
	buf[1] = val;
	bcm2835_spi_transfern(buf, 2);
}

void as3935_setMASKDIST(uint8_t val)
{
	DBGLN("as3935_setMASKDIST: %s\n", (val==TRUE) ? "TRUE" : "FALSE");
	LOG("Set Mask Disturber: %s\n", (val==TRUE) ? "True" : "False");

	uint8_t reg = as3935_read(0x03) & 0xDF;
	as3935_write(0x03, reg  | ((val==1) << 5));
}

uint8_t  as3935_getMASKDIST()
{
	DBGLN("as3935_getMASKDIST: ...\n");
	uint8_t reg = as3935_read(0x03) & 0x20;
	 DBGLN("as3935_getMASKDIST: %s\n", (reg != 0) ? "TRUE" : "FALSE");
	return reg >> 5;
}

uint8_t as3935_getAntennaTune()
{
	DBGLN("as3935_getAntennaTune: ...\n");
	uint8_t reg = as3935_read(0x03) & 0xC0;
	DBGLN("as3935_getAntennaTune: RATIO_%d\n",  0x10 << (reg >> 6));
	return reg >> 6;
}

void as3935_setAntennaTune(uint8_t val)
{
	DBGLN("as3935_setAntennaTune: RATIO_%d\n", 0x10 << val);
	if (val > ANT_TUNING_DIV_RATIO_128) val = ANT_TUNING_DIV_RATIO_128;
	uint8_t reg = as3935_read(0x03) & 0xC0;
	as3935_write(0x03, reg  | (val << 6));
}

void as3935_ResetDefault(void)
{
	DBGLN("as3935_ResetDefault\n");
	LOG("Reset to defaults\n");
	as3935_write(0x3C, 0x96);
}

void as3935_CalRCO(void)
{
	DBGLN("as3935_CalRCO\n");
	LOG("Calib RCO\n");
	as3935_write(0x3D, 0x96);
}

void as3935_CalTRCO(void)
{
	DBGLN("as3935_CalTRCO\n");
	LOG("Calib TRCO\n");
	as3935_CalRCO();
	
	uint8_t reg = as3935_read(0x08) & 0xDF;
	as3935_write(0x08, reg  | 0x20);
	mssleep(2);
	as3935_write(0x08, reg);
	mssleep(2);
}

void as3935_powerUp(void)
{
	DBGLN("as3935_powerUp\n");
	LOG("Power UP\n");
	uint8_t reg = as3935_read(0x00) & 0xFE;
	as3935_write(0x00, reg);
}

void as3935_powerDown(void)
{

	DBGLN("as3935_powerDown\n");
	LOG("Power Down\n");
	uint8_t reg = as3935_read(0x00) & 0xFE;
	reg |= 0x01;
	as3935_write(0x00, reg);
}

void as3935_setIndoor()
{
	DBGLN("as3935_setIndoor\n");
	LOG("Set Indoor\n");
	uint8_t reg = as3935_read(0x00) & 0xC1;
	reg |= 0x24;
	as3935_write(0x00, reg);
}

int as3935_getIndoor()
{
	DBGLN("as3935_getIndoor\n");
	uint8_t reg = as3935_read(0x00) & 0x3E;
	return (reg == 0x24);
}

void as3935_setOutdoor()
{
	DBGLN("as3935_setOutdoor\n");
	LOG("Set Outdoor\n");
	uint8_t reg = as3935_read(0x00) & 0xC1;
	reg |= 0x1C;
	as3935_write(0x00, reg);
}

int as3935_getOutdoor()
{
	return (as3935_getIndoor() == 0);
}

void as3935_setWDTH(unsigned char val)
{
	DBG("as3935_setWDTH: %d\n", val);
	LOG("Set WDTH: %d\n", val);
	if (val> 0x0A) val =0x0A;

	uint8_t reg = as3935_read(0x01) & 0xF0;
	reg |= val;
	as3935_write(0x01, reg);
}

uint8_t as3935_getWDTH()
{
	DBGLN("as3935_getWDTH: ");
	uint8_t reg = as3935_read(0x01) & 0x0F;
	DBG("as3935_getWDTH:  %d\n", reg);
}

void as3935_setSREJ(unsigned char val)
{
	DBG("as3935_setSREJ: %d\n", val);
	LOG("Set SREJ: %d\n", val);
	if (val > 0x0B) val = 0x0B;

	uint8_t reg = as3935_read(0x02) & 0xF0;
	reg |= val;
	as3935_write(0x02, reg);
}

uint8_t as3935_getSREJ()
{
	DBGLN("as3935_getSREJ: ");
	uint8_t reg = as3935_read(0x02) & 0x0F;
	DBG("as3935_getSREJ:  %d\n", reg);
}

void as3935_setNFLEV(unsigned char val)
{
	DBGLN("as3935_setNFLEV: %d\n", val);
	if (val> 0x07) val = 0x07;

	uint8_t reg = as3935_read(0x01) & 0x8F;
	reg |= (val<<4);
	as3935_write(0x01, reg);

	as3935->cmd_set_noise_floor_level = as3935_getNFLEV();
}

uint8_t as3935_getNFLEV()
{
	DBGLN("as3935_getNFLEV:");
	uint8_t reg = as3935_read(0x01) & 0x70;
	DBG("as3935_getNFLEV: %d\n",  reg >> 4);
	return reg >> 4;
}

void as3935_setDISPLCO(unsigned char val)
{
	DBGLN("as3935_setDISPLCO: %d\n", val);
	if (val> 0x01) val =0x01;

	uint8_t reg = as3935_read(0x08) & 0x7F;
	reg |= (val<<7);
	as3935_write(0x08, reg);
}

uint8_t as3935_getDISPLCO()
{
	DBGLN("as3935_getDISPLCO:");
	uint8_t reg = as3935_read(0x08) & 0x80;
	DBG("as3935_getDISPLCO: %d\n",  reg >> 7);
	return reg >> 7;
}

uint8_t as3935_setTUNCAP(uint8_t val)
{
	DBGLN("as3935_setTUNCAP: %d\n", val);
	if (val> 0x0F) val =0x0F;

	uint8_t reg = as3935_read(0x08) & 0xF0;
	reg |= val;
	as3935_write(0x08, reg);
}

uint8_t as3935_getTUNCAP()
{
	DBGLN("as3935_getTUNCAP:");
	uint8_t reg = as3935_read(0x08) & 0x0F;
	DBG("as3935_getTUNCAP: %d\n",  reg);
	return reg;
}

uint8_t as3935_clearSTAT()
{
	DBGLN("as3935_clearSTAT\n");
	LOG("Clear STAT\n");
	uint8_t reg = as3935_read(0x02) & 0xBF;
	as3935_write(0x02, reg | (1<<6));
	usleep(1000);
	as3935_write(0x02, reg);
	usleep(1000);
	as3935_write(0x02, reg | (1<<6));
	usleep(1000);
}

uint8_t as3935_setMinNumLight(uint8_t  val)
{
	DBGLN("as3935_setMinNumLight: %d\n", minLightnig[val]);
	LOG("Set Min Num Light: %d\n", minLightnig[val]);
	if (val > 4) val = 4;
	uint8_t reg = as3935_read(0x02) & 0xE7;
	reg |= (val<<4);
	as3935_write(0x02, reg);
}

uint8_t as3935_getMinNumLight()
{
	DBGLN("as3935_getMinNumLight:");
	uint8_t reg = as3935_read(0x02) & 0x18;
	DBG("as3935_getMinNumLight: %d\n", minLightnig[reg]);
	return reg >> 4;
}

float as3935_getFreq()
{
	register uint32_t counter = 0;
	register wdt = 1000000;
	struct timeval start, stop;
        bcm2835_gpio_aren(GPIO_IRQ_PIN);
    	bcm2835_gpio_set_eds(GPIO_IRQ_PIN);
	gettimeofday(&start,NULL);
	while (1)
		{
			if (bcm2835_gpio_eds(GPIO_IRQ_PIN) != 0)
			{
				counter++;
				bcm2835_gpio_set_eds(GPIO_IRQ_PIN);
				wdt = 1000000;
			}
			wdt--;

			if (counter >= 31250)
			{
				gettimeofday(&stop,NULL);
				break;
			}

			if (wdt == 0)
			{
				DBGLN("as3935_getFreq: watchdog.");
				break;
			}
		}
	bcm2835_gpio_clr_aren(GPIO_IRQ_PIN);
	
	float us = (float)((stop.tv_sec-start.tv_sec)*1000000ULL+(stop.tv_usec-start.tv_usec));
	return (float)counter * (float)1000000 / (us);
}

void as3935_TuneAntenna()
{
	uint8_t i;
	uint8_t best_tune = 0;
	float best_tune_freq = 0;
	float freq;
	for (i = 0 ; i <= 0x0F ; i++)
	{
		DBGLN("-------------------------------\n");
		LOG("Tune antenna with %d pF ", i * 8);
		mssleep(10);
		as3935_setAntennaTune(ANT_TUNING_DIV_RATIO_16);
		as3935_setTUNCAP(i);
		as3935_setDISPLCO(TRUE);
		mssleep(2);
		freq = as3935_getFreq() * 16;
		as3935->ant_resonance[i] = freq;
		as3935_setDISPLCO(FALSE);
		mssleep(2);

		float delta_freq = ABS(freq - 500000.0);
		float delta_best_tune_freq = ABS(best_tune_freq - 500000.0);
		LOG("Freq is: %0.2f Hz, delta: %0.2f Hz", freq, delta_freq);

		if (delta_freq < delta_best_tune_freq)
		{
			best_tune = i;
			best_tune_freq = freq;
		        LOG(" *Best*");
		}

		LOG("\n");
		DBGLN("-------------------------------\n");
	}

	LOG("Best Tune is: %d pF, %0.2f Hz\n", best_tune * 8, best_tune_freq);
	as3935_setTUNCAP(best_tune);
}

void as3935_TuneNoiseFloorLevel()
{
	DBGLN("-------------------------------\n");
	LOG("Initial tune of noise floor level\n");
	int i;
	for (i = 0; i< 7;  i++)
	{
		DBG("Set NFL to %d uV rms\n", NFL_uVrms[as3935_getIndoor()][i]);
		as3935_setNFLEV(i);
		mssleep(2100);
		if (bcm2835_gpio_lev(GPIO_IRQ_PIN) == 0) break;
		
		//Datasheet specify a delay of min 2ms from INT to READ REGISTER
		usleep(2100);

		uint8_t reg = as3935_read(0x03) & 0x0F;
		DBGLN("Interrupt type: ");
			switch (reg)
			{
				case 0x01:
					DBG("Noise level too high\n");
					break;
				case 0x04:
					DBG("Disturber detected\n");
					break;
					DBG("Lightning\n");
					break;
			}
	}
	LOG("Set NFL to %d uV rms\n", NFL_uVrms[as3935_getIndoor()][as3935_getNFLEV()]);
}

void update_data(){
	as3935->indoor_settings= as3935_getIndoor();
	as3935->noise_floor_level = as3935_getNFLEV();
	as3935->watchdog_threshold = as3935_getWDTH();
	as3935->min_num_lightnings = as3935_getMinNumLight();
	as3935->spike_rejection = as3935_getSREJ();
	as3935->mask_disturber = as3935_getMASKDIST();
	as3935->ant_tuner_capacitor = as3935_getTUNCAP();

	if(as3935->cmd_set_noise_floor_level != as3935->noise_floor_level)
	 {
		as3935_setNFLEV(as3935->cmd_set_noise_floor_level);
	 }

	int i;
	for(i = 0; i< 0x33; i++)
		as3935->reg[i] = as3935_read(i);
}

int main (int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc ; i++)
	{
		if ((strcmp(argv[i],"--verbose") == 0) || (strcmp(argv[i],"-v") == 0))
			opt_verbose = 1;
		else if ((strcmp(argv[i],"--wait") == 0) || (strcmp(argv[i],"-w") == 0))
			opt_wait_seconds = atoi(argv[i + 1]);
		else if ((strcmp(argv[i],"--daemonize") == 0) || (strcmp(argv[i],"-d") == 0))
			opt_daemonize = 1;
		else if ((strcmp(argv[i],"--daemonkill") == 0) || (strcmp(argv[i],"-dk") == 0))
			opt_daemonkill = 1;
		else if ((strcmp(argv[i],"--query-daemon") == 0) || (strcmp(argv[i],"-q") == 0))
			opt_query_daemon  = 1;
		else if ((strcmp(argv[i],"--printf-json") == 0) || (strcmp(argv[i],"-pj") == 0))
			opt_print_json  = 1;
		else if ((strcmp(argv[i],"--query-command") == 0) || (strcmp(argv[i],"-qcmd") == 0))
			opt_query_command  = 1;
		else if ((strcmp(argv[i],"--help") == 0) || (strcmp(argv[i],"-?") == 0))
		{
			printf("Usage: readAS3935 [OPTION]\nRead data from AS3935\n");
			printf("\t-v, --verbose\t\tGive detailed error messages\n");
			printf("\t-r w, --wait\t\tWait n second for events if not running as a daemon\n");
			printf("\t-d, --daemon\t\tStart daemon core\n");
			printf("\t-dk, --daemonkill\t\tKill dameon core\n");
			printf("\t-q,\t\t--query-daemon\t\tQuery the daemon\n");
			printf("\t-qcmd, \t\t--query-command\\Query the command to the daemon\n");
			printf("\t-pj,\t\t--print-json\t\tPrint JSON output\n");
			printf("\t-?, --help\t\tShow usage information\n\n");
			exit(0);
		}
	}

	//data memory allocation
	if (opt_daemonize){
		daemonize();
		shmem_server_init(SHMEM_KEY, sizeof(struct as3935_t), (void**) &as3935);
	} else if (opt_query_daemon){
		shmem_client_init(SHMEM_KEY, sizeof(struct as3935_t), (void**) &as3935);
	} else {
		as3935 = malloc(sizeof(struct as3935_t));
	}

	if (!opt_query_daemon)
	{
		if (!bcm2835_init())
		{
			printf("bcm2835_init failed. Are you running as root??\n");
			exit(1);
		}
		if (!bcm2835_spi_begin())
		{
			printf("bcm2835_spi_begin failedg. Are you running as root??\n");
			exit(1);
		}
		bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
		bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);
		bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256); 
		bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
		bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

		as3935->log_counter = 0;
		as3935->log_pointer = 0;
		as3935->lightning_events_counter = 0;
		as3935->lightning_events_pointer = 0;
		memset(as3935->log[0].string, 0x00, LOG_STR_LEN);

		as3935_ResetDefault();
		as3935_clearSTAT();
		as3935_powerDown();
		as3935_TuneAntenna();
		as3935_CalTRCO();
		as3935_powerUp();
		//as3935_CalRCO(); chiamato dentro cal TRCO

		//as3935_clearSTAT();

		as3935_setMinNumLight(0);
		as3935_setMASKDIST(TRUE);
		as3935_setIndoor();
		as3935_setWDTH(0);
		as3935_setSREJ(0);

		//as3935_TuneNoiseFloorLevel();

		update_data();

		DBGLN("-------------------------------\n");
		struct timeval start, stop, nfl_adj;
		gettimeofday(&start,NULL);
		while (1)
			{
				if (opt_daemonize)
				{
					gettimeofday(&stop,NULL);
					if (bcm2835_gpio_lev(GPIO_IRQ_PIN))
					{
						char buffer[26];

						//Datasheet specify a delay of min 2ms from INT to READ REGISTER
						usleep(2100);
						uint8_t reg = as3935_read(0x03) & 0x0F;
						LOG("Interrupt (0x%02x):", reg);
						DBGLN("Interrupt type (0x02x): ", reg);
						switch (reg)
						{
							case 0x01:
								LOG("Noise level too high ->");
								LOG("Increase NFL to %d uV rms\n", NFL_uVrms[as3935_getIndoor()][MIN(0x07, as3935_getNFLEV(i) + 1)]);
								as3935_setNFLEV(as3935_getNFLEV(i) + 1);
								gettimeofday(&nfl_adj,NULL);
								break;
							case 0x04:
								LOG("Disturber detected\n");
								break;
							case 0x08:
								gettimeofday(&(as3935->lightning_events[as3935->lightning_events_pointer].timestamp), NULL);
								as3935->lightning_events[as3935->lightning_events_pointer].distance = as3935_read(0x07) & 0x3F;
								as3935->lightning_events[as3935->lightning_events_pointer].energy = ((as3935_read(0x06) & 0x1F) << 16) | (as3935_read(0x05) << 8) | as3935_read(0x04);
								as3935->lightning_events_pointer++;
								as3935->lightning_events_counter++;
								if(as3935->lightning_events_pointer >= MAX_LIGHT_EVENTS) as3935->lightning_events_pointer = 0;
								if(as3935->lightning_events_counter > MAX_LIGHT_EVENTS) as3935->lightning_events_counter = MAX_LOG_EVENTS;
								LOG("Lightning\n");
								break;
							default:
								LOG("Unknown %02x\n", reg);
						}
					}

					if (!opt_daemonize)
					{
						if  (stop.tv_sec - start.tv_sec > opt_wait_seconds)
							{
								LOG("Timeout. Exiting...\n");
								break;
							}
					}
					usleep(2000);
					
					//Call update data every 2s
                                        if ((stop.tv_sec - nfl_adj.tv_sec) > 30){
					   if(as3935_getNFLEV(i) > 0) 
						{
                                                        LOG("Try to reduce NFL to %d uV rms\n", NFL_uVrms[as3935_getIndoor()][MIN(0x07, as3935_getNFLEV(i) - 1)]);
							as3935_setNFLEV(as3935_getNFLEV(i) - 1);
							gettimeofday(&nfl_adj,NULL);
						}
					};
					if (((stop.tv_sec - start.tv_sec) % 2) == 1) update_data();

			}
		}
		as3935_powerDown();
		bcm2835_spi_end();
		bcm2835_close();
	}

	if (opt_query_command){
		as3935->cmd_tune_antenna = 0;
		as3935->cmd_set_noise_floor_level = 0;
	}

	if (opt_query_daemon){
		printf("\n");
		printf("{\n");
		printf("\t\"noise_floor_level_ndx\":\"%d\",\n", as3935->noise_floor_level);
		printf("\t\"indoor_settings\":\"%d\",\n", as3935->indoor_settings);
		printf("\t\"noise_floor_level\":\"%d\",\n", NFL_uVrms[as3935->indoor_settings][as3935->noise_floor_level]);

		printf("\t\"watchdog_threshold\":\"%d\",\n", as3935->watchdog_threshold);
		printf("\t\"min_num_lightnings\":\"%d\",\n", as3935->min_num_lightnings);
		printf("\t\"spike_rejection\":\"%d\",\n", as3935->spike_rejection);
		printf("\t\"mask_disturber\":\"%d\",\n", as3935->mask_disturber);
		printf("\t\"ant_tuner_capacitor\":\"%d\",\n", as3935->ant_tuner_capacitor);


		printf("\t\"ant_resonance\": [\n");
		int i;
			for (i=0; i < 0x0F; i++)
			{	
				printf("\t\t{");
				printf("\"cap\":\"%dpF\", \"freq\":\"%.1fHz\"", i * 8, as3935->ant_resonance[i]);
				printf("}");
				if(i < 0x0F - 1) printf(",");
				printf("\n");
			}
		printf("\t],\n");

                printf("\t\"lightnings\": [\n");
                
                        for (i=0; i < as3935->lightning_events_counter; i++)
                        {
				int ptr = (as3935->lightning_events_pointer - i - 1)  % MAX_LIGHT_EVENTS;
				time_t nowtime;
				struct tm *nowtm;
				char tmpbuf[64];

				nowtime = as3935->lightning_events[ptr].timestamp.tv_sec;
				nowtm = localtime(&nowtime);
				strftime(tmpbuf, sizeof tmpbuf, "%Y-%m-%d %H:%M:%S", nowtm);

                                printf("\t\t{");
				printf("\"timestamp\":\"%s.%06d\"", tmpbuf, as3935->lightning_events[ptr].timestamp.tv_usec);
				printf(",");
				printf("\"energy\":\"%08d\"", as3935->lightning_events[ptr].energy);
				printf(",");
				printf("\"distance\":\"%02dKm\"", as3935->lightning_events[ptr].distance);
				printf("}");
				if (i < as3935->lightning_events_counter - 1) printf(",");
                                printf("\n");
                        }
                printf("\t],\n");

                printf("\t\"log\": [\n");
                        for (i = 0; i < as3935->log_counter ; i++)
                        {
                                int ptr = (as3935->log_pointer - i - 1) % MAX_LOG_EVENTS;
                                time_t nowtime;
                                struct tm *nowtm;
                                char tmpbuf[64];

                                nowtime = as3935->log[ptr].timestamp.tv_sec;
                                nowtm = localtime(&nowtime);
                                strftime(tmpbuf, sizeof tmpbuf, "%Y-%m-%d %H:%M:%S", nowtm);

                                printf("\t\t\{");
				printf("\"timestamp\":\"%s.%06d\"", tmpbuf, as3935->log[ptr].timestamp.tv_usec);
				printf(",");
                                printf("\"msg\":\"%s\"", as3935->log[ptr].string);
				printf("}");
                                if (i < as3935->log_counter - 1) printf(",");
                                printf("\n");
                        }
                printf("\t],");

                printf("\t\"reg\": [\n");
                
                        for (i = 0; i < 0x33; i++)
                        {
                                printf("\t\t{");
                                printf("\"reg%02x\":\"0x%02x\"", i, as3935->reg[i]);
                                printf("}");
                                if (i < 0x33 - 1) printf(",");
                                printf("\n");
                        }
                printf("\t]\n");

		printf("}\n");
	}

	return 0;
}
