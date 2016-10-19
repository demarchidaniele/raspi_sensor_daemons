#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <bcm2835.h>

#include "common.h"
#include "shmem.h"
#include "daemon.h"

#define CONVERT_MS(a) ((float)(a) / 10)
#define CONVERT_KMH(a) (((float)(a) / 10) * 3600 / 1000)
#define CONVERT_DEG(a)  (fmod(  ( (float)(a) * 22.5 + opt_degrees_offset  )    , 360.0))
#define SHMEM_KEY 2323

uint8_t opt_gpio_pin = 17;
//uint8_t opt_gpio_poweron_pin = 0;
uint8_t opt_verbose = 0;
uint8_t opt_speed_ms = 0;
uint8_t opt_speed_kmh = 0;
uint8_t opt_degrees = 0;
uint32_t opt_degrees_offset = 0;
uint8_t opt_daemonize = 0;
uint8_t opt_update_cycle = 5;
uint8_t opt_samples = 5;
uint8_t opt_query_daemon = 0;
uint8_t opt_retry = 1;
uint8_t opt_print_sampling = 0;
uint8_t opt_print_speedmax_only = 0;
uint8_t opt_print_speedmin_only = 0;
uint8_t opt_print_speedave_only = 0;
uint8_t opt_print_direction_only = 0;
uint8_t opt_print_json = 0;

const char TX23_Directions[16][4] = {{"N"}, {"NNE"}, {"NE"}, {"ENE"}, {"E"}, {"ESE"}, {"SE"}, {"SSE"}, {"S"}, {"SSW"}, {"SW"}, {"WSW"}, {"W"}, {"WNW"}, {"NW"}, {"NNW"}};

struct t_tx23{
	uint8_t gpio_pin;
	uint8_t gpio_poweron_pin;

	struct t_sample{
		struct t_bit_sampling{
				uint8_t bitval;
				uint32_t timestamp;
		} bit_sample[64];

		uint8_t bit_samples;
		uint8_t bit_counter;
		uint8_t error;

		uint32_t startFrameTime;
		uint32_t bitWidth;

		uint8_t header;
		uint32_t winddir;
		uint32_t winddir2;
		uint32_t windspeed;
		uint32_t windspeed2;
		uint32_t checksum;
		uint32_t checksumCalc;
		uint8_t isValidData;
	} values_samples[64];

	uint8_t samples_pointer;
	uint8_t samples_counter;
} *tx23;

void readTX23(struct t_sample* sample);
void print_sampling();
void print_values();

void readTX23(struct t_sample* sample)
{
	//uint32_t timeout = 2000000;
	/*This function will pull the data line low, then measure the number
	of microseconds to every state change of the data pin.
	It is an infinite loop.
	*/

	struct timeval start, stop;
	uint32_t i=0;
	uint32_t pinstate = 0;
	uint64_t timestamp;

	//Reset bit samples
	sample->bit_samples = 0;

	//Pull the DATA Pin low for 100ms to signal TX23 to send data
	bcm2835_gpio_fsel(opt_gpio_pin, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(opt_gpio_pin, LOW);
	delay(500);
	gettimeofday(&start,NULL);
	bcm2835_gpio_fsel(opt_gpio_pin, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_set_pud(opt_gpio_pin, BCM2835_GPIO_PUD_OFF);
	while (1)
		{
		gettimeofday(&stop,NULL);
		timestamp = (uint64_t) ((stop.tv_sec - start.tv_sec) * 1000000ULL + (stop.tv_usec - start.tv_usec));
		if (  bcm2835_gpio_lev(opt_gpio_pin) != pinstate )
			{
				pinstate ^= 1;
				sample->bit_sample[sample->bit_samples].timestamp = timestamp;
				sample->bit_sample[sample->bit_samples].bitval = pinstate;
				sample->bit_samples++;
				i++;
			}
		if ((timestamp > 100000ULL)  ||  (i >= 63))
			break;
		}
	sample->startFrameTime = sample->bit_sample[2].timestamp;
	sample->bitWidth= (sample->bit_sample[4].timestamp - sample->bit_sample[2].timestamp) / 3;
}

uint8_t TX23_GetBit(struct t_sample* sample, uint8_t bit)
{
	int bitTime = sample->startFrameTime + sample->bitWidth / 2 + (bit * sample->bitWidth);

	int i = 0;
	while(i < 62)
	{
		if (bitTime < sample->bit_sample[i + 1].timestamp) 
			return sample->bit_sample[i].bitval;
		i++;
	}

	return 0;
}

void CalcValues(struct t_sample* sample)
{
	sample->header = 0;
	sample->winddir = 0;
	sample->winddir2 = 0;
	sample->windspeed = 0;
	sample->windspeed2 = 0;
	sample->checksum = 0;

	int bit = 0;
	for (bit = 0; bit < 5; bit++)
	sample->header |= TX23_GetBit(sample, bit + 0)<<bit; 

	for (bit = 0; bit < 4; bit++)
	sample->winddir |= TX23_GetBit(sample, bit + 5)<<bit; 

	for (bit = 0; bit < 12; bit++)
	sample->windspeed |= TX23_GetBit(sample, bit + 9)<<bit; 

	for (bit = 0; bit < 4; bit++)
	sample->checksum |= TX23_GetBit(sample, bit + 21)<<bit; 

	for (bit = 0; bit < 4; bit++)
	sample->winddir2 |= (TX23_GetBit(sample, bit + 25) ^ 1)<<bit; 

	for (bit = 0; bit < 12; bit++)
	sample->windspeed2|= (TX23_GetBit(sample, bit + 29) ^ 1)<<bit; 

	sample->checksumCalc = 0;
	sample->checksumCalc += (sample->winddir & 15);
	sample->checksumCalc += ((sample->windspeed >> 8) & 15);
	sample->checksumCalc += ((sample->windspeed >> 4) & 15);
	sample->checksumCalc += (sample->windspeed & 15);

	sample->isValidData = TRUE;
	//if (data->values_samples[pointer].checksum != data->values_samples[pointer].checksumCalc) \
	//	{DBGLN("Checksum Error\n"); data->values_samples[pointer].isValidData = FALSE;}

	if (sample->header != 0x1B) \
		{DBGLN("Header Error\n"); sample->isValidData = FALSE;}	
	if (sample->winddir != sample->winddir2) \
		{DBGLN("Winddir Error\n"); sample->isValidData = FALSE;}
	if (sample->windspeed != sample->windspeed2) \
		{DBGLN("Windspeed Error\n"); sample->isValidData = FALSE;}
}

void print_sampling()
{
	DBGLN("print_sampling\n");
	int dataPtr = 0;
	int i, j;

	//find the last n_samples pointer
	int samples_counter = tx23->samples_counter;
	int new_opt_samples = MIN(opt_samples, samples_counter);
	int last_n_samples_pointer = (tx23->samples_pointer - new_opt_samples);
	if (last_n_samples_pointer < 0) last_n_samples_pointer += 64;

	DBGLN("samples_counter: %d\n", samples_counter);
	DBGLN("new_opt_samples: %d\n", new_opt_samples);
	DBGLN("last_n_samples_pointer: %d\n", last_n_samples_pointer);

	printf("Print Sampling\n\n");
	for(j = 0; j < new_opt_samples ; j++) {
		int sample_pointer = (j + last_n_samples_pointer) % 64;
		printf("Sample #%03d\n", sample_pointer);

		printf("Captured frame:\n");
		for (i = 0;  i < tx23->values_samples[sample_pointer].bit_samples; i++)
		{
			printf("(%d): %d us\n", \
					tx23->values_samples[sample_pointer].bit_sample[i].bitval, \
					tx23->values_samples[sample_pointer].bit_sample[i].timestamp);
		}

		printf("\n");
		printf("Decoded frame:\n");
		printf("Init frame: ");
		for (i=0; i<5; i++)
			printf("%d", TX23_GetBit(&(tx23->values_samples[sample_pointer]), i));
		printf("\n");

		printf("Wind Dir: ");
		for (; i<9; i++)
			printf("%d", TX23_GetBit(&(tx23->values_samples[sample_pointer]), i));
		printf("\n");

		printf("Wind Speed: ");
		for (; i<21; i++)
			printf("%d", TX23_GetBit(&(tx23->values_samples[sample_pointer]), i));
		printf("\n");

		printf("Checksum: ");
		for (; i<25; i++)
			printf("%d", TX23_GetBit(&(tx23->values_samples[sample_pointer]), i));
		printf("\n");

		printf("Wind Dir Inverted: ");
		for (; i<29; i++)
			printf("%d", TX23_GetBit(&(tx23->values_samples[sample_pointer]), i));
		printf("\n");

		printf("Wind Speed Inverted: ");
		for (; i<41; i++)
			printf("%d", TX23_GetBit(&(tx23->values_samples[sample_pointer]), i));

		printf("\n");

		if (tx23->values_samples[sample_pointer].isValidData == TRUE)
			printf("Data is VALID\n");
		else
			printf("Data is NOT VALID\n");

		printf("Checksum: (%d)\n", 	tx23->values_samples[sample_pointer].checksum);
		printf("Checksum Calc: (%d)\n", tx23->values_samples[sample_pointer].checksumCalc);

		if (opt_speed_ms) {
				printf("Wind Speed  %4.2f m/s\n", CONVERT_MS(tx23->values_samples[sample_pointer].windspeed));
				VERBOSE("Wind Speed2 %4.2f m/s\n", CONVERT_MS(tx23->values_samples[sample_pointer].windspeed2));
			} else if (opt_speed_kmh) {
				printf("Wind Speed  %6.2f km/h\n", CONVERT_KMH(tx23->values_samples[sample_pointer].windspeed));
				VERBOSE("Wind Speed2 %6.2f km/h\n", CONVERT_KMH(tx23->values_samples[sample_pointer].windspeed2));
			} else {
				printf("Wind Speed %5d\n", tx23->values_samples[sample_pointer].windspeed);
				VERBOSE("Wind Speed %5d\n", tx23->values_samples[sample_pointer].windspeed2);
			}

		if (opt_degrees)
		{
			printf("Wind Direction   %5.1f°\n", CONVERT_DEG(tx23->values_samples[sample_pointer].winddir));
			VERBOSE("Wind Direction2  %5.1f°\n", CONVERT_DEG(tx23->values_samples[sample_pointer].winddir2));
		}
		else
		{
			printf("Wind Direction  %s(%d)\n", TX23_Directions[tx23->values_samples[sample_pointer].winddir], tx23->values_samples[sample_pointer].winddir);
			VERBOSE("Wind Direction2 %s(%d)\n", TX23_Directions[tx23->values_samples[sample_pointer].winddir2], tx23->values_samples[sample_pointer].winddir2);
		}
		printf("\n");
	}
}

void print_values()
{
	int32_t winddir_max = 0;
	int32_t winddir_ave = 0;


	uint32_t windspeed_max = 0;
	uint32_t windspeed_min = 65535;
	uint32_t windspeed_ave = 0;

	uint8_t Errors = 0;
	uint8_t NoErrors = 0;

	//find the last n_samples pointer
	int samples_counter = tx23->samples_counter;
	int new_opt_samples = MIN(opt_samples, samples_counter);
	int last_n_samples_pointer = (tx23->samples_pointer - new_opt_samples);
	if (last_n_samples_pointer < 0) last_n_samples_pointer += 64;

	DBGLN("samples_counter: %d\n", samples_counter);
	DBGLN("new_opt_samples: %d\n", new_opt_samples);
	DBGLN("last_n_samples_pointer: %d\n", last_n_samples_pointer);

	int i = 0;
	for (i = 0; i < new_opt_samples; i++)
	{
		int sample_pointer = (i + last_n_samples_pointer) % 64;
		if(tx23->values_samples[sample_pointer].isValidData == FALSE)
			Errors++;
		else
		{
			NoErrors++;
			if (tx23->values_samples[sample_pointer].windspeed > windspeed_max) windspeed_max = tx23->values_samples[sample_pointer].windspeed;
			if (tx23->values_samples[sample_pointer].windspeed < windspeed_min) windspeed_min = tx23->values_samples[sample_pointer].windspeed;
			windspeed_ave += tx23->values_samples[sample_pointer].windspeed;
			winddir_ave += tx23->values_samples[sample_pointer].winddir;
		}
	}

	if (NoErrors){
		windspeed_ave /= NoErrors;
		winddir_ave /= NoErrors;
	}
		if (opt_print_json) {
			printf("\n");
			printf("{\n");
			printf("\t\"samples_ok\":\"%d\",\n", NoErrors);
			printf("\t\"samples_errors\":\"%d\",\n", Errors);
				printf("\t\"wind_speed_max_ms\":\"%.2f\",\n", CONVERT_MS(windspeed_max));
				printf("\t\"wind_speed_min_ms\":\"%.2f\",\n", CONVERT_MS(windspeed_min));
				printf("\t\"wind_speed_ave_ms\":\"%.2f\",\n", CONVERT_MS(windspeed_ave));
				printf("\t\"wind_speed_max_kmh\":\"%.2f\",\n", CONVERT_KMH(windspeed_max));
				printf("\t\"wind_speed_min_kmh\":\"%.2f\",\n", CONVERT_KMH(windspeed_min));
				printf("\t\"wind_speed_ave_kmh\":\"%.2f\",\n", CONVERT_KMH(windspeed_ave));
				printf("\t\"wind_speed_max_raw\":\"%d\",\n", windspeed_max);
				printf("\t\"wind_speed_min_raw\":\"%d\",\n", windspeed_min);
				printf("\t\"wind_speed_ave_raw\":\"%d\",\n", windspeed_ave);
				printf("\t\"wind_dir_raw\":\"%d\",\n", winddir_ave);
				printf("\t\"wind_dir_deg\":\"%.1f\",\n", CONVERT_DEG(winddir_ave));
				printf("\t\"wind_dir_cardinals_ndx\":\"%d\",\n", ((int)winddir_ave) % 17);
				printf("\t\"wind_dir_cardinals\":\"%s\"\n", TX23_Directions[((int)winddir_ave) % 17]);
				printf("}\n");
		} else if (opt_print_speedmax_only) {
				if (opt_speed_ms) {
					printf("%.2f\n", CONVERT_MS(windspeed_max));
				} else if (opt_speed_kmh){
					printf("%.2f\n", CONVERT_KMH(windspeed_max));
				} else {
					printf("%d\n", windspeed_max);
				}
		} else if (opt_print_speedmin_only) {
				if (opt_speed_ms) {
					printf("%.2f\n", CONVERT_MS(windspeed_min));
				} else if (opt_speed_kmh){
					printf("%.2f\n", CONVERT_KMH(windspeed_min));
				} else {
					printf("%d\n", windspeed_min);
				}
		}else if (opt_print_speedave_only) {
				if (opt_speed_ms) {
					printf("%.2f\n", CONVERT_MS(windspeed_ave));
				} else if (opt_speed_kmh){
					printf("%.2f\n", CONVERT_KMH(windspeed_ave));
				} else {
					printf("%d\n", windspeed_ave);
				}
		} else if(opt_print_direction_only){
			if (opt_degrees)
				printf("%.1f\n", CONVERT_DEG(winddir_ave));
			else
				printf("%d\n", winddir_ave);
		} else {
			printf("Summary\nvalues_samples_counter = %d\n" , tx23->samples_counter );
			printf("Errors %d/%d\n", Errors, Errors + NoErrors);

			if (opt_speed_ms) {
				printf("Max Wind Speed %4.2f m/s\n", CONVERT_MS(windspeed_max));
				printf("Min Wind Speed %4.2f m/s\n", CONVERT_MS(windspeed_min));
				printf("Average Wind Speed %4.2f m/s\n", CONVERT_MS(windspeed_ave));
			} else if (opt_speed_kmh){
				printf("Max Wind Speed %6.2f km/h\n", CONVERT_KMH(windspeed_max));
				printf("Min Wind Speed %6.2f km/h\n", CONVERT_KMH(windspeed_min));
				printf("Average Wind Speed %6.2f km/h\n", CONVERT_KMH(windspeed_ave));
			} else {
				printf("Max Wind Speed %5d\n", windspeed_max);
				printf("Min Wind Speed %5d\n", windspeed_min);
				printf("Average Wind Speed %5d\n", windspeed_ave);
			}

			if (opt_degrees)
				printf("Average Wind Direction  %5.1f°\n", CONVERT_DEG(winddir_ave));
			else
				printf("Average Wind Direction %s(%d)\n", TX23_Directions[winddir_ave % 17], winddir_ave);
		}
}

int main (int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc ; i++)
	{
		if ((strcmp(argv[i],"--verbose") == 0) || (strcmp(argv[i],"-v") == 0))
			opt_verbose = 1;
		else if ((strcmp(argv[i],"--gpio") == 0) || (strcmp(argv[i],"-gpio") == 0))
			 opt_gpio_pin = atoi(argv[i + 1]);
		//else if ((strcmp(argv[i],"--gpio_poweron") == 0) || (strcmp(argv[i],"-gpon") == 0))
		//	 opt_gpio_poweron_pin = atoi(argv[i + 1]);
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
		else if ((strcmp(argv[i],"--print-sampling") == 0) || (strcmp(argv[i],"-psa") == 0))
			 opt_print_sampling = 1;
		else if ((strcmp(argv[i],"--print-json") == 0) || (strcmp(argv[i],"-pj") == 0))
			 opt_print_json = 1;
		else if ((strcmp(argv[i],"--print-speedmax-only") == 0) || (strcmp(argv[i],"-psmax") == 0))
			 opt_print_speedmax_only = 1;
		else if ((strcmp(argv[i],"--print-speedmin-only") == 0) || (strcmp(argv[i],"-psmin") == 0))
			 opt_print_speedmin_only = 1;
		else if ((strcmp(argv[i],"--print-speedave-only") == 0) || (strcmp(argv[i],"-psave") == 0))
			 opt_print_speedave_only = 1;
		else if ((strcmp(argv[i],"--print-direction-only") == 0) || (strcmp(argv[i],"-pd") == 0))
			 opt_print_direction_only = 1;
		else if ((strcmp(argv[i],"--speed_ms") == 0) || (strcmp(argv[i],"-ms") == 0))
			 opt_speed_ms  = 1;
		else if ((strcmp(argv[i],"--speed_kmh") == 0) || (strcmp(argv[i],"-kmh") == 0))
			 opt_speed_kmh  = 1;
		else if ((strcmp(argv[i],"--degrees") == 0) || (strcmp(argv[i],"-deg") == 0))
			 opt_degrees  = 1;
		else if ((strcmp(argv[i],"--degrees-offset") == 0) || (strcmp(argv[i],"-dego") == 0))
			opt_degrees_offset = atoi(argv[i + 1]);

		else if ((strcmp(argv[i],"--help") == 0) || (strcmp(argv[i],"-?") == 0))
		{
			printf("Usage: tx23read [OPTION]\nRead data from a La Crosse TX23 Anemometer.\n");
			printf("\t-v,\t\t--verbose\t\t\tGive detailed messages\n");
			printf("\t-gpio,\t\t--gpio\t\t\t\tSelect GPIO port (default 17)\n");
			//printf("\t-gpon,\t\t--gpon\t\t\t\tSelect GPIO port to Power ON of the sensor (default none)\n");
			printf("\t-r [0..10],\t--retry[0..10]\t\t\tRetry counter\n");
			printf("\t-d,\t\t--daemonize\t\t\tStart daemon with update cycle data refresh\n");
			printf("\t-uc [5..60],\t--update-cycle [5..60]\t\tSet update cycle (default 3)\n");
			printf("\t-s [1..64],\t--samples [1..64]\t\tSet samples (default 5)\n");
			printf("\t-d,\t\t--query-daemon\t\t\tQuery the daemon\n");

			printf("\t-psa,\t\t--print-sampling\t\tPrint sampling of input pin\n");
			printf("\t-psmax,\t\t--print-speedmax-only\t\tPrint wind speed max only\n");
			printf("\t-psmin,\t\t--print-speedmin-only\t\tPrint wind speed min only\n");
			printf("\t-psave,\t\t--print-speedave-only\t\tPrint wind speed average only\n");
			printf("\t-pd,\t\t--print-direction-only\t\tPrint wind direction only\n");
			printf("\t-pj,\t\t--print-json\t\t\tJSON output\n");

			printf("\t-ms,\t\t--speed_ms\t\t\tPrint speed in m/s\n");
			printf("\t-kmh,\t\t--speed_kmh\t\t\tPrint speed in Km/h\n");
			printf("\t-deg,\t\t--degrees\t\t\tPrint direction in degrees from north\n");
			printf("\t-dego [0..360],\t--degrees-offset [0..360] \tCorrect Offset degrees from north\n");

			printf("\t-?,\t\t--help\t\t\t\tShow usage information\n\n");
			exit(0);
		}
	}

	//Correct parameters settings
	//Oprtions check
	if (!opt_query_daemon) {
		if(opt_gpio_pin <= 0) {
			printf("Please select a valid GPIO pin\n");
			return 3;
		}
		//if(opt_gpio_poweron_pin < 0) {
		//	printf("Please select a valid GPIO Power ON pin\n");
		//	return 3;
		//}
	}
	if((opt_samples <= 0) || (opt_samples > 64)) {
		printf("Invalid number of samples\n");
		return 3;
	}

	if(opt_update_cycle < 5) opt_update_cycle = 5;
	if (opt_query_daemon) opt_daemonize = 0;

	//data memory allocation
	if (opt_daemonize){
		daemonize();
		shmem_server_init(SHMEM_KEY, sizeof(struct t_tx23), (void**) &tx23);
	} else if (opt_query_daemon){
		shmem_client_init(SHMEM_KEY, sizeof(struct t_tx23), (void**) &tx23);
	} else {
		tx23 = malloc(sizeof(struct t_tx23));
	}

	VERBOSE("readTX23\nLa Crosse TX23 utility\n");
	VERBOSE("(C) Daniele De Marchi - 2016\n");
	VERBOSE("GPIO Pin: GPIO%2d\n", opt_gpio_pin);
	VERBOSE("Update Cycle: %ds\n", opt_update_cycle);

	//if(opt_gpio_poweron_pin) VERBOSE("GPIO Power ON Pin: GPIO%2d\n", opt_gpio_poweron_pin);

	unsigned long  update_delay;
	if (!opt_query_daemon)
	{
		tx23->gpio_pin = opt_gpio_pin;
		//tx23->gpio_poweron_pin = opt_gpio_poweron_pin;
		tx23->samples_counter = 0;
		tx23->samples_pointer = 0;

		// Set the DATA pin to input
		if (!bcm2835_init())
		{
			printf("bcm2835_init failed. Are you running as root??\n");
			return 1;
		} 

		do{
			struct timeval start, stop;
			gettimeofday(&start,NULL);

			uint8_t retry_counter = 0;
			readTX23(&(tx23->values_samples[tx23->samples_pointer]));
			CalcValues(&(tx23->values_samples[tx23->samples_pointer]));
			//retry loop
			while((tx23->values_samples[tx23->samples_pointer].isValidData == FALSE) && (retry_counter < opt_retry))
			{
				DBGLN("Retry %d\n", retry_counter + 1);
				usleep(5000000);
				readTX23(&(tx23->values_samples[tx23->samples_pointer]));
				CalcValues(&(tx23->values_samples[tx23->samples_pointer]));
				retry_counter++;
				gettimeofday(&start,NULL);
			}

			if(tx23->values_samples[tx23->samples_pointer].isValidData == FALSE) 
			{
				usleep(5000000);
				gettimeofday(&start,NULL);
			}

			tx23->samples_counter++;
			if(tx23->samples_counter >= 64) tx23->samples_counter = 64;

			tx23->samples_pointer++;
			if(tx23->samples_pointer >= 64) tx23->samples_pointer = 0;

			gettimeofday(&stop,NULL);
			unsigned long  update_delay = (unsigned long) ((stop.tv_sec-start.tv_sec) * 1000000ULL+(stop.tv_usec-start.tv_usec));;
			while ((update_delay < opt_update_cycle  * 1000000ULL) && \
					((tx23->samples_counter < opt_samples) || opt_daemonize))
			{ //wait loop
				gettimeofday(&stop,NULL);
			 	update_delay = (unsigned long) ((stop.tv_sec-start.tv_sec) * 1000000ULL+(stop.tv_usec-start.tv_usec));
				usleep(100 * 1000);
			}
			DBGLN("samples_counter: %02d\n", tx23->samples_counter);
		} while((tx23->samples_counter < opt_samples) || opt_daemonize);
		} // query daemon
	
	if (opt_print_sampling) print_sampling();
	print_values();
	
	return 0;
}
