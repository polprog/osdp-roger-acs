#include <stdio.h>
#include <termios.h>
#include <time.h>
#include "osdp.h"

#ifndef READER_ADDR
#define READER_ADDR 0x7f
#endif

#define SIGNAL_OK  1
#define SIGNAL_ERR 2
#define SIGNAL_PIN 3
#define SIGNAL_PIN_QUIET 4

#define MODE_CARD         1
#define MODE_CARD_AND_PIN 2
#define MODE_CARD_OR_PIN  3

#define QUERY_CARD 1
#define QUERY_PIN  2

#define USERNAMELEN 256
#define USERPINLEN  64
#define MD5LEN      32+1
struct userdata {
	char name[USERNAMELEN];
	char card_id[USERPINLEN];
	char pin[MD5LEN]; //md5 sum!
	uint32_t door_list;
};


void getUserFromCardData(struct userdata *user, char* doorname, struct osdp_response *res, sqlite3 **db, char* state, char querytype);
void printHelp();
void reader_signal(char signal, int ttyfd);
void open_door(int32_t mask);


void getUserFromCardData(struct userdata *user, char* doorname, struct osdp_response *res, sqlite3 **db, char* state, char querytype){

	if(res->payloadlen > USERPINLEN){
		LOG_PRINT_ERROR("Payload length from reader too big!");
		return;
	}
	char* cardStrPos = user->card_id;
	for(int i=4; i < res->payloadlen; i++){
		sprintf(cardStrPos, "%02X", res->payload[i]);
		cardStrPos += 2;
	}

	
	DPRINT("res.payload: size=%di cardStr=%s", res->payloadlen, user->card_id); 
	DPRINT("SEARCH IN DB: card = (%s)\n", user->card_id);
	snprintf(dataBuf, BUF_SIZE,
		"SELECT distinct users.name, users.card, users.pin, doors.name, users.expire_date FROM users JOIN user_zone JOIN zones JOIN doors JOIN zone_door ON ( "
		"users.userid = user_zone.userid AND user_zone.zoneid = zones.zoneid AND doors.doorid = zone_door.doorid AND zone_door.zoneid = zones.zoneid AND "
		"doors.name LIKE '%s' AND "
		"users.%s = '%s' );", doorname,
		(querytype == QUERY_CARD) ? "card" : "pin",        
		(querytype == QUERY_CARD) ? user->card_id : user->pin
		);

	DPRINT("SQL: %s\n", dataBuf);
	int ret;
	sqlite3_stmt *stmt;
	ret = sqlite3_prepare_v2( *db, dataBuf, -1, &stmt, NULL );
	const char *userName = NULL;
	const char *userPin = NULL;
	int expire = 0;
	
	while ( ( ret = sqlite3_step(stmt) ) == SQLITE_ROW ) {
		userName = (const char*) sqlite3_column_text(stmt, 0);
		userPin = (const char*) sqlite3_column_text(stmt, 2);
		expire = sqlite3_column_int(stmt, 4);
		int curr_time = time(0);
		DPRINT("FIND IN DB: %s -> %d vs %d\n", userName, expire, curr_time);
		if (curr_time < expire){
			strncpy(user->name, userName, USERNAMELEN);
			strncpy(user->pin, userPin, MD5LEN);
		}else{
			break; //return empty user->name
		}
		char* door = (char*) sqlite3_column_text(stmt, 3);
		//int pietro = atoi(door+12);
		char* procent = strstr(doorname, "%");
		int pietro = 0;
		if(procent != NULL){
			pietro = atoi(door+(procent-doorname));
		}

		DPRINT("FIND IN DB: %s -> %d \n", door, pietro);
		user->door_list |= (1<<pietro);
	}
		
}

void open_door(int32_t mask){
	DPRINT("\nOtwieram drzwi: %04X\n\n", mask);
}

int main(int argc, char** argv){
	int portTXfd;
	struct termios optionsTX;
	#ifdef _USE_RS232
	const bool use_rs485 = 0;
	#else
	const bool use_rs485 = 1;
	#endif //_USE_RS232

	struct osdp_packet packet, answer;
	struct osdp_response res;
	char state;
	char mode;
	int code_timeout, code_timeout_max=10;
	if(argc < 3){
		printHelp(argv);
		exit(1);
	}
	
	if(strncmp("ONLY_CARD", argv[2], strlen("ONLY_CARD")) == 0){
		mode = MODE_CARD;
		syslog(LOG_INFO, "Running with mode CARD");
	}else if(strncmp("CARD_AND_PIN", argv[2], strlen("CARD_AND_PIN")) == 0){
		mode = MODE_CARD_AND_PIN;
		syslog(LOG_INFO, "Running with mode CARD_AND_PIN");
	}else if(strncmp("CARD_OR_PIN", argv[2], strlen("CARD_OR_PIN")) == 0){
		mode = MODE_CARD_OR_PIN;
		syslog(LOG_INFO, "Running with mode CARD_OR_PIN");
	}else{
		printHelp(argv);
		exit(1);
	}
	
	// open syslog
	openlog("modbus_reader", LOG_PID, LOG_DAEMON);
	
	// open serial port to reader
	portTXfd = portsetup(TTY_PORT, &optionsTX, use_rs485);
	
	sqlite3 *db;
	if ( sqlite3_open(DATABESE_FILE, &db) ) {
		syslog(LOG_CRIT, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		exit(-1);
	}
	
	state = WAIT_FOR_FIRST_ID;
	code_timeout = -1;
	
	struct userdata user;
	char pin_buffer[USERPINLEN], *pin_buffer_ptr = pin_buffer;
	memset(&user, 0, sizeof(user));
	memset(&pin_buffer, 0, sizeof(pin_buffer));
	while(1){
		DPRINT("timer=%d\tstate=%d\n", code_timeout, state);
		fill_packet(&packet, READER_ADDR, osdp_POLL, NULL, 0);
		send_packet(&packet, portTXfd);
		#ifdef OSDP_DEBUG
		packet_dump(&packet);
		#endif
		//sleep(1);
		usleep(500000);
		if(recv_packet(&answer, portTXfd)){
			process_packet(&answer, &res); //packet_dump(&answer);
			DPRINT("RES code= %02xh len= %d\n", res.response, res.payloadlen);
			switch(res.response){
				case osdp_ACK:
					if(state == HAS_CARD){
						reader_signal(SIGNAL_PIN_QUIET, portTXfd);
						if(code_timeout > 0){
							code_timeout--;
							DPRINT("code_timeout--\n");
						}else{
							DPRINT("Code timeout elapsed\n");
							state = WAIT_FOR_FIRST_ID;
							memset(&user, 0, sizeof(user));
							memset(&pin_buffer, 0, sizeof(pin_buffer));
							pin_buffer_ptr = pin_buffer;
						}
					}
					break;
				case osdp_NACK:
					LOG_PRINT_WARN("Received NACK from reader\n");
					break;
				case osdp_RAW:
					if(state != WAIT_FOR_FIRST_ID || state == HAS_CARD) break;
					//card data received. check db to get user
					/* check db here */
					memset(&user, 0, sizeof(user));
					getUserFromCardData(&user, argv[1], &res, &db, &state, QUERY_CARD);
					DPRINT("Got user by card: %s\t%s\n", user.name, user.card_id);
					if(strcmp(user.name, "") == 0){
						reader_signal(SIGNAL_ERR, portTXfd);
						DPRINT("No user for this card\n");
						state = WAIT_FOR_FIRST_ID;
						code_timeout = -1;
						break;
					}
					DPRINT("MODE=%d", mode);
					if(mode == MODE_CARD || mode == MODE_CARD_OR_PIN){
						open_door(user.door_list);
						
						state = WAIT_FOR_FIRST_ID;
						reader_signal(SIGNAL_OK, portTXfd);
					}else{
						reader_signal(SIGNAL_PIN, portTXfd);
						state = HAS_CARD; code_timeout = code_timeout_max;
					}
					break;
				case osdp_KPD:
					//received pin data
					if(state != HAS_CARD && mode == MODE_CARD_OR_PIN){
						/* signal error to user, use card first */
						reader_signal(SIGNAL_ERR, portTXfd);
						break;
					}
					// sprawdzamy pin
					for(int i = 0; i < res.payloadlen; i++){
						if(pin_buffer_ptr - pin_buffer >= USERPINLEN-1) {
							break;
						}
						if(res.payload[i] == 0x7f){ // * na pinpadzie
							memset(&pin_buffer, 0, sizeof(pin_buffer));
							pin_buffer_ptr = pin_buffer;
						}else if(res.payload[i] == 0x0d){ //# na pinpoadzie
							DPRINT("Sprawdzam pin...\n");
							char pin_md5[MD5LEN];
							unsigned char digest[16];
							MD5_CTX context;
							MD5_Init(&context);
							MD5_Update(&context, pin_buffer, strlen(pin_buffer));
							MD5_Final(digest, &context);
			
							for(i = 0; i < 16; ++i) {
								sprintf(pin_md5 + i*2, "%02x", (unsigned int)digest[i]);
							}
							pin_md5[32] = 0;
							DPRINT("Wprowadzono pin %s\n", pin_buffer);
							DPRINT("Wprowadzono md5 %s:%s prawidlowy\n", pin_md5, user.pin);
							int j;
							if(mode == MODE_CARD_AND_PIN){
								j = strncmp(pin_md5, user.pin, MD5LEN);
							}else{
								memset(&user, 0, sizeof(user));
								strncpy(user.pin, pin_md5, MD5LEN);
								getUserFromCardData(&user, argv[1], &res, &db, &state, QUERY_PIN);
								j = !strncmp(user.name, "", USERNAMELEN);
							}
							DPRINT("j = %d", j);
							if(j == 0){
								DPRINT("\n\n\tOK\n\n");
								/* otworz drzwi */
								state = WAIT_FOR_FIRST_ID;
								open_door(user.door_list);
								reader_signal(SIGNAL_OK, portTXfd);
							}else{
								/* czerwone lampki */
								DPRINT("\n\n\tblad\n\n");
								state = WAIT_FOR_FIRST_ID;
								reader_signal(SIGNAL_ERR, portTXfd);
							}

							pin_buffer_ptr = pin_buffer;
							break;	
						}else {
							*pin_buffer_ptr = res.payload[i];
							pin_buffer_ptr++;
						}
						DPRINT("Dopisuje %c do pinu, %s\n", res.payload[i], pin_buffer);
						code_timeout = code_timeout_max;
					}
	
					break;


			}
		}else{
			fprintf(stderr, "Received bad packet\n");
			tcflush(portTXfd, TCIOFLUSH);
			close(portTXfd);
			
			portTXfd = portsetup(TTY_PORT, &optionsTX, use_rs485);
			printf("Port reopepend and initialized\n");
		}
	}

}


void printHelp(char **argv){
	fprintf(stderr, "Usage: %s <doorname> <type>\n", argv[0]);
	fprintf(stderr, "where type is:\n");
	fprintf(stderr, "\tONLY_CARD\n");
	fprintf(stderr, "\tCARD_AND_PIN\n");
	fprintf(stderr, "\tCARD_OR_PIN\n");
	
}

void reader_signal(char signal, int ttyfd){
	struct osdp_packet packet;
	switch(signal){
		case SIGNAL_OK:
			ledset(&packet, READER_ADDR, 1, GREEN, false, 48);
			send_packet(&packet, ttyfd);
			usleep(50000);
	recv_packet(&packet, ttyfd);
			beepset(&packet, READER_ADDR, 1, 1, 1);
			send_packet(&packet, ttyfd);
			break;
		case SIGNAL_ERR:
			ledset(&packet, READER_ADDR, 1, RED, true, 24);
			send_packet(&packet, ttyfd);
			usleep(50000);
	recv_packet(&packet, ttyfd);
			beepset(&packet, READER_ADDR, 3, 1, 3);
			send_packet(&packet, ttyfd);
			break;
		case SIGNAL_PIN:
			ledset(&packet, READER_ADDR, 1, AMBER, true, 12);
			send_packet(&packet, ttyfd);
			usleep(50000);
	recv_packet(&packet, ttyfd);
			beepset(&packet, READER_ADDR, 1, 1, 2);
			send_packet(&packet, ttyfd);
			break;
		case SIGNAL_PIN_QUIET:
			ledset(&packet, READER_ADDR, 1, AMBER, true, 12);
			send_packet(&packet, ttyfd);
			break;
		default:
			return;
	}
	usleep(50000);
	recv_packet(&packet, ttyfd);
}
