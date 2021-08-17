#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "error_reporting.h"
#include "door_controller.h"
#include "gpios/gpio.h"
#include "user_db.h"
#include "eventSend.h"

Door    *doors;
int      doorsCount;
sqlite3 *database;

void printHelp(char **argv){
	fprintf(stderr, "Usage: %s -d <database> -n <doorname> -m <mode> -s <serial> [-a <reader_addres>] [-r] [-e <event_host>] [-l <log_host> -L <log_port>]\n", argv[0]);
	fprintf(stderr, "Usage: %s -d <database> -c <config_file> [-e <event_host>] [-l <log_host> -L <log_port>]\n", argv[0]);
	fprintf(stderr, "where <database> is path to sqlite database\n");
	fprintf(stderr, "<doorname> is name of door for this controller\n");
	fprintf(stderr, "<mode> is:\n");
	fprintf(stderr, "\t%d for card only\n", MODE_CARD);
	fprintf(stderr, "\t%d for pin only \n", MODE_PIN);
	fprintf(stderr, "\t%d for card OR pin\n", MODE_CARD_OR_PIN);
	fprintf(stderr, "\t%d for card AND pin\n", MODE_CARD_AND_PIN);
	fprintf(stderr, "\t%d for card AND pin without exceptions\n", MODE_CARD_AND_PIN_ALWAYS);
	fprintf(stderr, "\t%d for keys access controller\n", MODE_TWO_CARDS);
	fprintf(stderr, "<serial> is /dev/tty... device (with reader connection)\n");
	fprintf(stderr, "<reader_addres> is reader address (default 0x%x)\n", defaultAddress);
	fprintf(stderr, "-r enable rs485 mode on <serial> device (transmitter control via RTS signal)\n");
	fprintf(stderr, "<event_host> is address (IP or domain) to send HTTP event\n");
	fprintf(stderr, "<log_host> and <log_port> is address (IP or domain) and port to send TCP access log info\n");
}

static struct option long_options [] = {
	{"database-file",  required_argument, NULL, 'd'},
	{"door-name",      required_argument, NULL, 'n'},
	
	{"reader-config",  required_argument, NULL, 'c'},
	{"reader-mode",    required_argument, NULL, 'm'},
	{"reader-serial",  required_argument, NULL, 's'},
	{"reader-addr",    required_argument, NULL, 'a'},
	{"reader-rs485",   no_argument,       NULL, 'r'},
	
	{"event-host",     required_argument, NULL, 'e'},
	{"log-host",       required_argument, NULL, 'l'},
	{"log-port",       required_argument, NULL, 'L'},
	{0, 0, 0, 0}
};

char* configParse(char **b, const char *sep, const char *configFile, int line) {
	char *f = strtok_r(*b, " \t\n", b);
	if (!f) {
		fprintf(stderr, "Error parse config file %s on line %d\n", configFile, line);
		exit(1);
	}
	return f;
}

int main(int argc, char **argv) {
	// open syslog
	openlog("door_controller", LOG_PID, LOG_DAEMON);
	
	int ret;
	const char *databasePath = NULL;
	const char *configFile = NULL;
	
	const char *doorName = NULL;
	uint8_t accessMode = 0;
	const char *serialDev = NULL;
	uint8_t readerAddres = defaultAddress;
	bool use_rs485 = false;
	
	// arg parse
	while((ret = getopt_long(argc, argv, "d:n:c:m:s:a:re:l:L:", long_options, 0)) != -1) {
		switch (ret) {
			case 'd':
				databasePath = optarg;
			break;
			
			case 'c':
				configFile = optarg;
			case 'n':
				doorName = optarg;
			break;
			case 'm':
				accessMode = strtoll(optarg, 0, 0);
			break;
			case 's':
				serialDev = optarg;
			case 'a':
				readerAddres = strtoll(optarg, 0, 0);
			break;
			case 'r':
				use_rs485 = true;
			break;
			
			case 'e':
				eventHost = optarg;
			break;
			case 'l':
				logHost = optarg;
			break;
			case 'L':
				logPort = optarg;
			break;
		}
	}
	
	if(!databasePath || (!configFile && (!doorName || !serialDev))) {
		printHelp(argv);
		exit(1);
	}
	
	if (configFile) {
		char line[256];
		FILE *config;
		config=fopen(configFile, "r");
		if (!config) {
			fprintf(stderr, "Can't open %s: %s\n", configFile, strerror(errno));
			exit(1);
		}
		
		doorsCount = 0;
		while(fgets(line, 256, config)) {
			if (line[0] >= '0')
				++doorsCount;
		}
		
		if(doorsCount<1) {
			fprintf(stderr, "Can't find doors in config file %s\n", configFile);
			exit(1);
		}
		
		doors = malloc(doorsCount * sizeof(Door));
		memset(doors, 0, doorsCount * sizeof(Door));
		
		fseek(config, 0, SEEK_SET);
		int lineCount = 1;
		Door *door = doors;
		while(fgets(line, 256, config)) {
			if (line[0] < '0')
				continue;
			
			char *b = line, *serialDev_A, *serialDev_B;
			uint8_t readerAddres_A, readerAddres_B, use_rs485_A, use_rs485_B;
			
			door->doorName = strdup(configParse(&b, " \t\n", configFile, lineCount));
			
			door->readerA.accessMode = strtoll(configParse(&b, " \t\n", configFile, lineCount) , 0, 0);
			serialDev_A     = configParse(&b, " \t\n", configFile, lineCount);
			use_rs485_A     = strtoll(configParse(&b, " \t\n", configFile, lineCount) , 0, 0);
			readerAddres_A  = strtoll(configParse(&b, " \t\n", configFile, lineCount) , 0, 0);
			
			door->readerB.accessMode = strtoll(configParse(&b, " \t\n", configFile, lineCount) , 0, 0);
			serialDev_B     = configParse(&b, " \t\n", configFile, lineCount);
			use_rs485_B     = strtoll(configParse(&b, " \t\n", configFile, lineCount) , 0, 0);
			readerAddres_B  = strtoll(configParse(&b, " \t\n", configFile, lineCount) , 0, 0);
			
			door->maskOffset = strtoll(configParse(&b, " \t\n", configFile, lineCount) , 0, 0);
			door->maskFull   = strtoll(configParse(&b, " \t\n", configFile, lineCount) , 0, 0);
			
			door->lastInput  = DI_STATE_IS_DIFF | 0x0f;
			
			LOG_PRINT_INFO("read door %s from config with "
				"accessMode_A=%d serialDev_A=%s use_rs485_A=%d readerAddres_A=0x%x "
				"accessMode_B=%d serialDev_B=%s use_rs485_B=%d readerAddres_B=0x%x "
				"maskOffset=%d maskFull=0x%x",
				door->doorName,
				door->readerA.accessMode, serialDev_A, use_rs485_A, readerAddres_A,
				door->readerB.accessMode, serialDev_B, use_rs485_B, readerAddres_B,
				door->maskOffset, door->maskFull
			);
			
			// initialize reader(s)
			if (door->readerA.accessMode) {
				door->readerA.reader = init_reader(serialDev_A, use_rs485_A, readerAddres_A, door);
				if (!door->readerA.reader) {
					exit(2);
				}
			}
			if (door->readerB.accessMode) {
				door->readerB.reader = init_reader(serialDev_B, use_rs485_B, readerAddres_B, door);
				if (!door->readerB.reader) {
					exit(2);
				}
			}
			
			door->door_lock_timer = 1;
			++lineCount;
			++door;
		}
		
		fclose(config);
		
		LOG_PRINT_INFO("Start %s with databasePath=%s configFile=%s eventHost=%s logHost=%s logPort=%s",
			argv[0], databasePath, configFile, eventHost, logHost, logPort
		);
	} else {
		if(accessMode != MODE_CARD && accessMode != MODE_PIN && accessMode != MODE_CARD_OR_PIN && accessMode != MODE_CARD_AND_PIN && accessMode != MODE_CARD_AND_PIN_ALWAYS && accessMode != MODE_TWO_CARDS) {
			fprintf(stderr, "Invalid mode (%d)\n", accessMode);
			printHelp(argv);
			exit(1);
		}
		
		doors = malloc(sizeof(Door));
		memset(doors, 0, sizeof(Door));
		Door *door = doors;
		
		door->doorName = doorName;
		door->readerA.accessMode = accessMode;
		door->maskFull = 0x0f;
		door->lastInput  = DI_STATE_IS_DIFF | 0x0f;
		door->door_lock_timer = 1;
		
		// initialize reader
		door->readerA.reader = init_reader(serialDev, use_rs485, readerAddres, door);
		if (!door->readerA.reader) {
			exit(2);
		}
		
		LOG_PRINT_INFO("Start %s with databasePath=%s doorName=%s accessMode=%d serialDev=%s  eventHost=%s logHost=%s logPort=%s",
			argv[0], databasePath, doorName, accessMode, serialDev, eventHost, logHost, logPort
		);
	}
	
	// open database
	if ( sqlite3_open(databasePath, &database) ) {
		LOG_PRINT_CRIT("Can't open database: %s\n", sqlite3_errmsg(database));
		sqlite3_close(database);
		exit(3);
	}
	
	// main loop
	while(1) {
		mainLoop(doors, doorsCount, database);
	}
}
