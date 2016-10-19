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

#define DHT11 11
#define DHT22 22
#define SHMEM_KEY 2302
#define VERSION 0.1

unsigned char opt_type = 22;
unsigned char opt_verbose = 0;
unsigned char opt_retry = 1;
unsigned char opt_print_sampling = 0;
unsigned char opt_daemonize = 0;
unsigned char opt_query_daemon = 0;
unsigned char opt_gpio_pin = 22;
unsigned char opt_gpio_poweron_pin = 0;
unsigned char opt_update_cycle = 10;
unsigned char opt_samples = 1;
unsigned char opt_print_temperature_only = 0;
unsigned char opt_print_humidity_only = 0;
unsigned char opt_print_json = 0;

struct t_dht{
	unsigned char sensor_type;
	unsigned char gpio_pin;
	unsigned char gpio_poweron_pin;

	struct t_sample{
		struct t_bit_sampling{
				unsigned char bitval;
				unsigned int timestamp;
		} bit_sample[256];
		unsigned char data[5];
		unsigned char bit_samples;
		unsigned char bit_counter;
		unsigned char error;

		float temperature;
		float humidity;
	} samples[64];

	unsigned char samples_counter;
	unsigned char samples_pointer;
} *dht;

void readDHT(struct t_sample* sample);
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
		else if ((strcmp(argv[i],"--gpio") == 0) || (strcmp(argv[i],"-gpio") == 0))
			 opt_gpio_pin = atoi(argv[i + 1]);
		else if ((strcmp(argv[i],"--gpio_poweron") == 0) || (strcmp(argv[i],"-gpon") == 0))
			 opt_gpio_poweron_pin = atoi(argv[i + 1]);
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
		else if ((strcmp(argv[i],"--print-temperature-only") == 0) || (strcmp(argv[i],"-pt") == 0))
			 opt_print_temperature_only = 1;
		else if ((strcmp(argv[i],"--print-humidity-only") == 0) || (strcmp(argv[i],"-ph") == 0))
			 opt_print_humidity_only = 1;
		else if ((strcmp(argv[i],"--print-json") == 0) || (strcmp(argv[i],"-pj") == 0))
			 opt_print_json=1;
		else if ((strcmp(argv[i],"--help") == 0) || (strcmp(argv[i],"-?") == 0))
		{
			printf("readDHT version %f\n", VERSION);
			printf("Usage: readDHT [OPTION]\nRead data from a DHT11 or DHT22 sensor\n");
			printf("\t-v,\t\t--verbose\t\tGive detailed messages\n");
			printf("\t-t\t\t--type\t\t\tSpecify DTH type [11 or 22] (default 22)\n");
			printf("\t-gpio,\t\t--gpio\t\t\tSelect GPIO port (default 22)\n");
			printf("\t-gpon,\t\t--gpon\t\t\tSelect GPIO port to Power ON of the sensor (default none)\n");
			printf("\t-r [0..10],\t\t--retry[0..10]\t\t\tRetry counter\n");
			printf("\t-d,\t\t--daemonize\t\tStart daemon with update cycle data refresh\n");
			printf("\t-uc [5..60],\t--update-cycle [10..60]\tSet update cycle (default 3)\n");
			printf("\t-s [1..255],\t--samples [1..64]\tSet average samples num(default 1)\n");
			printf("\t-q,\t\t--query-daemon\t\tQuery the daemon\n");
			printf("\t-psa,\t\t--print-sampling\tPrint sampling of input pin\n");
			printf("\t-pt,\t\t--print-temperature-only\tPrint temperature only\n");
			printf("\t-ph,\t\t--print-humidity-only\tPrint humudity only\n");
			printf("\t-pj,\t\t--print-json\t\tJSON output\n");

			printf("\t-?,\t\t--help\t\t\tShow usage information\n\n");
			exit(0);
		}
	}

	//Options check
	if (!opt_query_daemon) {
		if(opt_gpio_pin <= 0) {
			printf("Please select a valid GPIO pin\n");
			return 3;
		}
		if(opt_gpio_poweron_pin < 0) {
			printf("Please select a valid GPIO Power ON pin\n");
			return 3;
		}
	}
	if((opt_samples <= 0) || (opt_samples > 64)) {
		printf("Invalid number of samples\n");
		return 3;
	}
	if ((opt_type != 11) && (opt_type != 22))
	{
			printf("Please select a valid sensor type: 11 or 22\n");
			return 3;
	}

	if(opt_update_cycle < 10) opt_update_cycle = 10;
	if (opt_query_daemon) opt_daemonize = 0;

	//data memory allocation
	if (opt_daemonize){
		daemonize();
		shmem_server_init(SHMEM_KEY, sizeof(struct t_dht), (void**) &dht);
	} else if (opt_query_daemon){
		shmem_client_init(SHMEM_KEY, sizeof(struct t_dht), (void**) &dht);
	} else {
		dht = malloc(sizeof(struct t_dht));
	}

	VERBOSE("readDHT\nDHT Sensors utility\n");
	VERBOSE("(C) Daniele De Marchi - 2016\n");
	VERBOSE("Sensor type: DHT%02d\n", dht->sensor_type);
	VERBOSE("GPIO Pin: GPIO%2d\n", opt_gpio_pin);
	VERBOSE("Update Cycle: %ds\n", opt_update_cycle);

	if(opt_gpio_poweron_pin) VERBOSE("GPIO Power ON Pin: GPIO%2d\n", opt_gpio_poweron_pin);


	if (!opt_query_daemon)
	{
		dht->sensor_type = opt_type;
		dht->gpio_pin = opt_gpio_pin;
		dht->gpio_poweron_pin = opt_gpio_poweron_pin;
		dht->samples_counter = 0;
		dht->samples_pointer = 0;
		

		//Init BCM2835
		DBGLN("Init bcm2835\n");
		if (!bcm2835_init())
		{
			printf("bcm2835_init failed. Are you running as root??\n");
			return 1;
		} 

		do{ //main loop
			struct timeval start, stop;
			gettimeofday(&start,NULL);

			unsigned char retry_counter = 0;
			readDHT(&(dht->samples[dht->samples_pointer]));
			//retry loop
			while ((dht->samples[dht->samples_pointer].error != 0) && (retry_counter < opt_retry)){
				DBGLN("Retry %d\n", retry_counter + 1);
				usleep(1000000);
				readDHT(&(dht->samples[dht->samples_pointer]));
				retry_counter++;
				gettimeofday(&start,NULL);
			} 

			if(dht->samples[dht->samples_pointer].error == FALSE) 
			{
				usleep(1000000);
				gettimeofday(&start,NULL);
			}

			dht->samples_counter++;
			if(dht->samples_counter >= 64) dht->samples_counter = 64;

			dht->samples_pointer++;
			if(dht->samples_pointer >= 64) dht->samples_pointer = 0;

			unsigned long  update_delay = (unsigned long) ((stop.tv_sec-start.tv_sec) * 1000000ULL+(stop.tv_usec-start.tv_usec));;
			while ((update_delay < opt_update_cycle  * 1000000ULL) && \
					((dht->samples_counter < opt_samples) || opt_daemonize))
			{ //wait loop
				gettimeofday(&stop,NULL);
			 	update_delay = (unsigned long) ((stop.tv_sec-start.tv_sec) * 1000000ULL+(stop.tv_usec-start.tv_usec));
				usleep(100 * 1000);
			}
			DBGLN("samples_counter: %02d\n", dht->samples_counter);
		} while((dht->samples_counter < opt_samples) || opt_daemonize);
		bcm2835_gpio_fsel(opt_gpio_poweron_pin, BCM2835_GPIO_FSEL_INPT);
	}//query daemon
	
	if (opt_print_sampling) print_sampling();
	print_values();
	
	return 0;
} // main



void readDHT(struct t_sample* sample){
	DBGLN("readDHT\n");
	int i;

	//Power ON PIN
	DBGLN("Set Power ON Pin: OFF\n");
	bcm2835_gpio_fsel(dht->gpio_poweron_pin, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(dht->gpio_poweron_pin, LOW);
	usleep(1000000);
	DBGLN("Set Power ON Pin: ON\n");
	bcm2835_gpio_write(dht->gpio_poweron_pin, HIGH);
	usleep(2000000);

	// Set GPIO pin to output
	DBGLN("Start reading command\n");
	bcm2835_gpio_fsel(dht->gpio_pin, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(dht->gpio_pin, HIGH);
	usleep(50000);
	bcm2835_gpio_write(dht->gpio_pin, LOW);
	usleep(20000);
	bcm2835_gpio_fsel(dht->gpio_pin, BCM2835_GPIO_FSEL_INPT);

	for (i = 0; i < 5; i++) sample->data[i] = 0;
	sample->bit_samples = 0;
	sample->bit_counter = 0;

	int laststate;
	int pinstate;
	unsigned int timestamp;
	laststate = FALSE;
	pinstate = FALSE;
	struct timeval start, stop;
	gettimeofday(&start,NULL);
	while (1)
	{
		gettimeofday(&stop, NULL);
		pinstate = bcm2835_gpio_lev(dht->gpio_pin);
		timestamp  =(unsigned int)((stop.tv_sec - start.tv_sec) * 1000000ULL + (stop.tv_usec - start.tv_usec));
		if (pinstate != laststate)
		{
			laststate = pinstate;
			sample->bit_sample[sample->bit_samples].timestamp = timestamp;
			sample->bit_sample[sample->bit_samples].bitval = pinstate;
			sample->bit_samples++;
			if (sample->bit_samples > 255) sample->bit_samples = 255;
		}
		if (timestamp > 8000) break; 
	}

	int dataPtr = 0;

	int bitlen;
	i = 0;

	//Check the first 2/3 bits to avoid initial transitions and/or the 80us starting frames
	bitlen = sample->bit_sample[i + 1].timestamp - sample->bit_sample[i].timestamp;
	if((bitlen < 40) || ( bitlen > 70)) i++;

	bitlen = sample->bit_sample[i + 1].timestamp - sample->bit_sample[i].timestamp;
	if((bitlen < 40) || ( bitlen > 70)) i++;

	bitlen = sample->bit_sample[i + 1].timestamp - sample->bit_sample[i].timestamp;
	if((bitlen < 40) || ( bitlen > 70)) i++;

	bitlen = sample->bit_sample[i + 1].timestamp - sample->bit_sample[i].timestamp;
	if((bitlen < 40) || ( bitlen > 70)) i++;

	for(; i < sample->bit_samples - 1; i++)
	{
		bitlen = sample->bit_sample[i + 1].timestamp - sample->bit_sample[i].timestamp;
		if ((sample->bit_sample[i].bitval) && (bitlen > 50)) {sample->data[dataPtr] <<= 1; sample->data[dataPtr]  |= 1;}
		if ((sample->bit_sample[i].bitval) && (bitlen <= 50)) {sample->data[dataPtr] <<= 1;}
 		
		if (sample->bit_sample[i].bitval)
		    {
			sample->bit_counter++;
			if ((sample->bit_counter % 8) == 0) dataPtr++;
			if (dataPtr > 4) dataPtr = 4;
		    }
	}

	if (sample->data[4] == ((sample->data[0] + sample->data[1] + sample->data[2] + sample->data[3]) & 0xFF)) {
		if (dht->sensor_type == DHT11){
			sample->temperature = sample->data[2];
			sample->humidity  = sample->data[0];
		}

		if (dht->sensor_type == DHT22) {
			sample->humidity = sample->data[0] * 256 + sample->data[1];
			sample->humidity /= 10.0;

			sample->temperature = (sample->data[2] & 0x7F) * 256 + sample->data[3];
			sample->temperature /= 10.0;
			if (sample->data[2] & 0x80)
				sample->temperature *= -1;
		}
		sample->error = 0;
	} else {
		sample->error = 1;
	}
	if (sample->bit_counter != 40) sample->error = 1;

	//Power ON PIN
DBGLN("Set Power ON Pin: OFF\n");
	bcm2835_gpio_write(dht->gpio_poweron_pin, LOW);
	bcm2835_gpio_fsel(dht->gpio_poweron_pin, BCM2835_GPIO_FSEL_INPT);
}

void print_sampling(){
	DBGLN("print_sampling\n");
	int dataPtr = 0;
	int i, j;

	//find the last n_samples pointer
	int samples_counter = dht->samples_counter;
	int new_opt_samples = MIN(opt_samples, samples_counter);
	int last_n_samples_pointer = (dht->samples_pointer - new_opt_samples);
	if (last_n_samples_pointer < 0) last_n_samples_pointer += 64;

	DBGLN("samples_counter: %d\n", samples_counter);
	DBGLN("new_opt_samples: %d\n", new_opt_samples);
	DBGLN("last_n_samples_pointer: %d\n", last_n_samples_pointer);

	printf("Print Sampling\n\n");
	for(j = 0; j < new_opt_samples ; j++) {
		int sample_pointer = (j + last_n_samples_pointer) % 64;
		printf("Sample #%03d\n", sample_pointer);
		for(i = 1; i < dht->samples[sample_pointer].bit_samples - 1; i++)
		{
			int bitlen = dht->samples[sample_pointer].bit_sample[i + 1].timestamp - dht->samples[sample_pointer].bit_sample[i].timestamp;
			printf("%02d : Bit = %d, time = %d, ",i,  dht->samples[sample_pointer].bit_sample[i].bitval, dht->samples[sample_pointer].bit_sample[i].timestamp);
			if (i < dht->samples[sample_pointer].bit_samples - 1) printf("bitlen  = %d us", bitlen);
			if ((dht->samples[sample_pointer].bit_sample[i].bitval) && (bitlen > 50)) printf(", bit = 1");
			if ((dht->samples[sample_pointer].bit_sample[i].bitval) && (bitlen <= 50)) printf(", bit = 0");
			printf("\n");
		}
		printf("Bits (%02d): 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", \
			dht->samples[sample_pointer].bit_counter, \
			dht->samples[sample_pointer].data[0], dht->samples[sample_pointer].data[1], \
			dht->samples[sample_pointer].data[2], dht->samples[sample_pointer].data[3], dht->samples[sample_pointer].data[4]);

		if (dht->samples[sample_pointer].error == 0) printf("%.1f°C %.1f\%Hr\n", dht->samples[sample_pointer].temperature, dht->samples[sample_pointer].humidity);

		printf("\n");
	}
}

void print_values(){
	DBGLN("print_values\n");
	
	float temperature, humidity;
	int counter = 0;
	int i;
	int errors = 0;
	temperature =0;
	humidity = 0;

	//find the last n_samples pointer
	int samples_counter = dht->samples_counter;
	int new_opt_samples = MIN(opt_samples, samples_counter);
	int last_n_samples_pointer = (dht->samples_pointer - new_opt_samples);
	if (last_n_samples_pointer < 0) last_n_samples_pointer += 64;

	DBGLN("samples_counter: %d\n", samples_counter);
	DBGLN("new_opt_samples: %d\n", new_opt_samples);
	DBGLN("last_n_samples_pointer: %d\n", last_n_samples_pointer);

	for(i = 0; i < new_opt_samples ; i++) {
		int sample_pointer = (i + last_n_samples_pointer) % 64;
		if(dht->samples[sample_pointer].error == 0){
			temperature += dht->samples[sample_pointer].temperature;
			humidity += dht->samples[sample_pointer ].humidity;
			counter++;
		} else 
		errors++;
	}
	VERBOSE("Errors: %02d/%02d\n", errors, new_opt_samples);
	temperature /= (float)counter;
	humidity /= (float)counter;

	if (opt_print_temperature_only)
		printf("%.1f\n", temperature);
	else if (opt_print_humidity_only)
		printf("%.1f\n", humidity);
	else if(opt_print_json)
		{
		printf("\n");
		printf("{");
		printf("\"temperature_unit\":\"°C\",\n");
		printf("\"humidity_unit\":\"%\",\n");
		printf("\"temperature\":\"%.1f\",\n", temperature);
		printf("\"humidity\":\"%.1f\"", humidity);
		printf("}");
	} else 
		printf("%.1f°C %.1f\%Hr\n", temperature, humidity);	
}
