#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <bcm2835.h>
#include <unistd.h>

#include "common.h"
#include "shmem.h"
#include "daemon.h"

#define SHMEM_KEY 1800
#define VERSION 0.1

#define I2C_SLAVE_ADDR 0x77
// Register Addresses
#define REG_CALIB 0xAA
#define REG_MEAS 0xF4
#define REG_MSB 0xF6
#define REG_LSB 0xF7
#define REG_ID 0xD0
//Control Register Address
#define CRV_TEMP 0x2E
#define CRV_PRES 0x34 
//Oversample setting [0..3]
#define OVERSAMPLE 3

unsigned char opt_type = 22;
unsigned char opt_verbose = 0;
unsigned char opt_retry = 1;
unsigned char opt_print_sampling = 0;
unsigned char opt_daemonize = 0;
unsigned char opt_query_daemon = 0;
unsigned char opt_update_cycle = 10;
unsigned char opt_samples = 1;
unsigned char opt_print_temperature_only = 0;
unsigned char opt_print_pressure_only = 0;
unsigned char opt_print_json = 0;

struct t_bmp180{
	uint8_t chip_id;
	uint8_t chip_ver;

	int16_t AC1;
	int16_t AC2;
	int16_t AC3;
	uint16_t AC4;
	uint16_t AC5;
	uint16_t AC6;
	int16_t B1;
	int16_t B2;
	int16_t MB;
	int16_t MC;
	int16_t MD;

	struct t_sample{
		uint8_t temperature_buf[2];
		uint8_t pressure_buf[3];
		unsigned char error;
		float temperature;
		float pressure;
	} samples[64];

	unsigned char samples_counter;
	unsigned char samples_pointer;
} *bmp180;

void readBMP180(struct t_sample* sample);
void print_sampling();
void print_values();

int main(int argc, char **argv) {
	int i;
	for (i = 1; i < argc ; i++)
	{
		if ((strcmp(argv[i],"--verbose") == 0) || (strcmp(argv[i],"-v") == 0))
			opt_verbose = 1;
		else if ((strcmp(argv[i],"--type") == 0) || (strcmp(argv[i],"-t") == 0))
			 opt_type = atoi(argv[i + 1]);
		else if ((strcmp(argv[i],"--retry") == 0) || (strcmp(argv[i],"-r") == 0))
			 opt_retry = atoi(argv[i + 1]);
		else if ((strcmp(argv[i],"--update-cycle") == 0) || (strcmp(argv[i],"-uc") == 0))
			 opt_update_cycle = atoi(argv[i + 1]);
		else if ((strcmp(argv[i],"--samples") == 0) || (strcmp(argv[i],"-s") == 0))
			 opt_samples = atoi(argv[i + 1]);
		else if ((strcmp(argv[i],"--daemonize") == 0) || (strcmp(argv[i],"-d") == 0))
			 opt_daemonize = 1;
		else if ((strcmp(argv[i],"--query-daemon") == 0) || (strcmp(argv[i],"-q") == 0))
			 opt_query_daemon  = 1;
		else if ((strcmp(argv[i],"--print-temperature-only") == 0) || (strcmp(argv[i],"-pt") == 0))
			 opt_print_temperature_only = 1;
		else if ((strcmp(argv[i],"--print-pressure-only") == 0) || (strcmp(argv[i],"-pp") == 0))
			 opt_print_pressure_only = 1;
		else if ((strcmp(argv[i],"--print-json") == 0) || (strcmp(argv[i],"-pj") == 0))
			 opt_print_json=1;
		else if ((strcmp(argv[i],"--help") == 0) || (strcmp(argv[i],"-?") == 0))
		{
			printf("readBMP180 version %f\n", VERSION);
			printf("Usage: readBMP180 [OPTION]\nRead data from a BMP180 sensor\n");
			printf("\t-v,\t\t--verbose\t\tGive detailed messages\n");
			printf("\t-r [0..10],\t\t--retry[0..10]\t\t\tRetry counter\n");
			printf("\t-d,\t\t--daemonize\t\tStart daemon with update cycle data refresh\n");
			printf("\t-uc [10..60],\t--update-cycle [10..60]\tSet update cycle (default 10)\n");
			printf("\t-s [1..64],\t--samples [1..64]\tSet average samples num(default 1)\n");
			printf("\t-q,\t\t--query-daemon\t\tQuery the daemon\n");
			printf("\t-pj,\t\t--print-json\t\tJSON output\n");
			printf("\t-?,\t\t--help\t\t\tShow usage information\n\n");
			exit(0);
		}
	}

	if((opt_samples <= 0) || (opt_samples > 64)) {
		printf("Invalid number of samples\n");
		return 3;
	}

	if(opt_update_cycle < 10) opt_update_cycle = 10;
	if (opt_query_daemon) opt_daemonize = 0;

	//data memory allocation
	if (opt_daemonize){
		daemonize();
		shmem_server_init(SHMEM_KEY, sizeof(struct t_bmp180), (void**) &bmp180);
	} else if (opt_query_daemon){
		shmem_client_init(SHMEM_KEY, sizeof(struct t_bmp180), (void**) &bmp180);
	} else {
		bmp180 = malloc(sizeof(struct t_bmp180));
	}

	VERBOSE("readBMP180\nBMP180 Pressure & Temperature Sensors utility\n");
	VERBOSE("(C) Daniele De Marchi - 2016\n");
	VERBOSE("Update Cycle: %ds\n", opt_update_cycle);
	if (opt_query_daemon){
		VERBOSE("Chip ID: %d\n", bmp180->chip_id);
		VERBOSE("Chip Version: %d\n", bmp180->chip_ver);
	}

	if (!opt_query_daemon)
	{
		bmp180->samples_counter = 0;
		bmp180->samples_pointer = 0;
		

		//Init BCM2835
		DBGLN("Init bcm2835\n");
		if (!bcm2835_init())
		{
			printf("bcm2835_init failed. Are you running as root??\n");
			return 1;
		} 

		if (!bcm2835_i2c_begin())
		{
			printf("bcm2835_i2c_begin failed. Are you running as root??\n");
			return 1;
		}

		bcm2835_i2c_setSlaveAddress(I2C_SLAVE_ADDR);
		// BCM2835_I2C_CLOCK_DIVIDER_150 	
		// 150 = 60ns = 1.666 MHz (default at reset) 
		bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_150);

		//Read Chip ID and Chip Version
		uint8_t buf[22];
		uint8_t regaddr[1];
		regaddr[0] = REG_ID;
		bcm2835_i2c_read_register_rs(regaddr, buf, 2); 	
		bmp180->chip_id = buf[0];
		bmp180->chip_ver = buf[1];
		VERBOSE("Chip ID: %d\n", bmp180->chip_id);
		VERBOSE("Chip Version: %d\n", bmp180->chip_ver);

		//Read calibration data from sensor
		regaddr[0] = REG_CALIB;
		bcm2835_i2c_read_register_rs(regaddr, buf, 22); 	

		bmp180->AC1 = (buf[0] << 8) | buf[1];
		bmp180->AC2 = (buf[2]<<8) | buf[3];
		bmp180->AC3 = (buf[4]<<8) | buf[5];
		bmp180->AC4 = (buf[6]<<8) | buf[7];
		bmp180->AC5 = (buf[8]<<8) | buf[9];
		bmp180->AC6 = (buf[10]<<8) | buf[11];
		bmp180->B1 = (buf[12]<<8) | buf[13];
		bmp180->B2 = (buf[14]<<8) | buf[15];
		bmp180->MB = (buf[16]<<8) | buf[17];
		bmp180->MC = (buf[18]<<8) | buf[19];
		bmp180->MD =( buf[20]<<8) | buf[21];

		VERBOSE("Calibration values:\n");
		VERBOSE("\tAC1: %d\n", bmp180->AC1);
		VERBOSE("\tAC2: %d\n", bmp180->AC2);
		VERBOSE("\tAC3: %d\n", bmp180->AC3);
		VERBOSE("\tAC4: %d\n", bmp180->AC4);
		VERBOSE("\tAC5: %d\n", bmp180->AC5);
		VERBOSE("\tAC6: %d\n", bmp180->AC6);
		VERBOSE("\tB1: %d\n", bmp180->B1);
		VERBOSE("\tB2: %d\n", bmp180->B2);
		VERBOSE("\tMB: %d\n", bmp180->MB);
		VERBOSE("\tMC: %d\n", bmp180->MC);
		VERBOSE("\tMD: %d\n", bmp180->MD);
		do{ //main loop
			struct timeval start, stop;
			gettimeofday(&start,NULL);

			unsigned char retry_counter = 0;
			readBMP180(&(bmp180->samples[bmp180->samples_pointer]));
			//retry loop
			while ((bmp180->samples[bmp180->samples_pointer].error != 0) && (retry_counter < opt_retry)){
				DBGLN("Retry %d\n", retry_counter + 1);
				usleep(1000000);
				readBMP180(&(bmp180->samples[bmp180->samples_pointer]));
				retry_counter++;
				gettimeofday(&start,NULL);
			} 

			if(bmp180->samples[bmp180->samples_pointer].error == FALSE) 
			{
				usleep(1000000);
				gettimeofday(&start,NULL);
			}

			bmp180->samples_counter++;
			if(bmp180->samples_counter >= 64) bmp180->samples_counter = 64;

			bmp180->samples_pointer++;
			if(bmp180->samples_pointer >= 64) bmp180->samples_pointer = 0;

			gettimeofday(&stop,NULL);
			unsigned long  update_delay = (unsigned long) ((stop.tv_sec-start.tv_sec) * 1000000ULL+(stop.tv_usec-start.tv_usec));;
			while ((update_delay < opt_update_cycle  * 1000000ULL) && \
					((bmp180->samples_counter < opt_samples) || opt_daemonize))
			{ //wait loop
				gettimeofday(&stop,NULL);
			 	update_delay = (unsigned long) ((stop.tv_sec-start.tv_sec) * 1000000ULL+(stop.tv_usec-start.tv_usec));
				usleep(100 * 1000);
			}
			DBGLN("samples_counter: %02d\n", bmp180->samples_counter);
		} while((bmp180->samples_counter < opt_samples) || opt_daemonize);

		//Reset I2C pin to an input
		//bcm2835_i2c_end();
	}//query daemon
	print_values();
	
	return 0;
} // main



void readBMP180(struct t_sample* sample){
	uint8_t buf[3];
	uint8_t regaddr[1];
	int32_t temperature, pressure;
	int32_t UT, UP, X1, X2, X3, B3, B5, B6, B62, P;
	uint32_t B4, B7;

	//Read Temeprature
	buf[0] = REG_MEAS;
	buf[1] = CRV_TEMP;
	bcm2835_i2c_write(buf, 2);
	usleep(5000);
	regaddr[0] = REG_MSB;
	bcm2835_i2c_read_register_rs(regaddr, &(sample->temperature_buf[0]), 2);
  	UT = (sample->temperature_buf[0] << 8) + sample->temperature_buf[1];

	// Read pressure
	buf[0] = REG_MEAS;
	buf[1] = CRV_PRES + (OVERSAMPLE << 6);
	bcm2835_i2c_write(buf, 2);
	usleep(40000);
	regaddr[0] = REG_MSB;
	bcm2835_i2c_read_register_rs(regaddr, &(sample->pressure_buf[0]), 3);
	UP = ((sample->pressure_buf[0] << 16) + (sample->pressure_buf[1] << 8) + sample->pressure_buf[2]) >> (8 - OVERSAMPLE);

	// Refine temperature
	X1 = ((UT - bmp180->AC6) * bmp180->AC5) >> 15;
	X2 = (bmp180->MC << 11) / (X1 + bmp180->MD);
	B5 = X1 + X2;
	sample->temperature = (B5 + 8) >> 4;
	DBG("temperature = %d\n", sample->temperature);

	//Refine pressure
	B6 = B5 - 4000;
	B62 = B6 * B6 >> 12;
	X1 = (bmp180->B2 * B62) >> 11;
	X2 = bmp180->AC2 * B6 >> 11;
	X3 = X1 + X2;
	B3 = (((bmp180->AC1 * 4 + X3) << OVERSAMPLE) + 2) >> 2;

	X1 = bmp180->AC3 * B6 >> 13;
	X2 = (bmp180->B1 * B62) >> 16;
	X3 = ((X1 + X2) + 2) >> 2;
	B4 = (bmp180->AC4 * (X3 + 32768)) >> 15;
	B7 = (UP - B3) * (50000 >> OVERSAMPLE);

	P = (B7 * 2) / B4;

	X1 = (P >> 8) * (P >> 8);
	X1 = (X1 * 3038) >> 16;
	X2 = (-7357 * P) >> 16;
	sample->pressure = P + ((X1 + X2 + 3791) >> 4);
	DBG("pressure = %d\n", sample->pressure);
	sample->error = 0;
}

void print_values(){
	DBGLN("print_values\n");
	
	float temperature, pressure;
	int counter = 0;
	int i;
	int errors = 0;
	temperature =0;
	pressure = 0;

	//find the last n_samples pointer
	int samples_counter = bmp180->samples_counter;
	int new_opt_samples = MIN(opt_samples, samples_counter);
	int last_n_samples_pointer = (bmp180->samples_pointer - new_opt_samples);
	if (last_n_samples_pointer < 0) last_n_samples_pointer += 64;

	DBGLN("samples_counter: %d\n", samples_counter);
	DBGLN("new_opt_samples: %d\n", new_opt_samples);
	DBGLN("last_n_samples_pointer: %d\n", last_n_samples_pointer);

	for(i = 0; i < new_opt_samples ; i++) {
		int sample_pointer = (i + last_n_samples_pointer) % 64;
		if(bmp180->samples[sample_pointer].error == 0){
			temperature += bmp180->samples[sample_pointer].temperature;
			pressure += bmp180->samples[sample_pointer ].pressure;
			counter++;
		} else 
		errors++;
	}
	VERBOSE("Errors: %02d/%02d\n", errors, new_opt_samples);
	temperature /= (float)counter;
	pressure /= (float)counter;

	if (opt_print_temperature_only)
		printf("%.1f\n", temperature / 10.0);
	else if (opt_print_pressure_only)
		printf("%.2f\n", pressure / 100.0);
	else if(opt_print_json)
		{
		printf("\n");
		printf("{");
		printf("\"temperature_unit\":\"°C\",\n");
		printf("\"pressure_unit\":\"mBar\",\n");
		printf("\"temperature\":\"%.1f\",\n", temperature / 10.0);
		printf("\"pressure\":\"%.2f\"", pressure / 100.0);
		printf("}");
	} else 
		printf("%.1f°C %.2fmBar\n", temperature / 10.0, pressure / 100.0);	
}
