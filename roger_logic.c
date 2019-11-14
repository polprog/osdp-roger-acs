#include "roger.h"

#define RED    0x4
#define GREEN  0xa
#define ORANGE 0x1

int main(int argc, char **argv) {
	// open syslog
	openlog("modbus_reader", LOG_PID, LOG_DAEMON);
	
	// initialize RS232
	int serial = init_tty("/dev/ttyS2", B9600);
	if ( serial < 0 ) {
		LOG_PRINT_CRIT("Open TTY ERROR\n");
		exit(-1);
	}
	
	// open database
	sqlite3 *db;
	if ( sqlite3_open(DATABESE_FILE, &db) ) {
		syslog(LOG_CRIT, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		exit(-1);
	}
	
	epso_write_read(serial, 0x00, 0xB1, 62, dataBuf, BUF_SIZE);
	epso_write_read(serial, 0x00, 0xe8, ORANGE, dataBuf, BUF_SIZE);
	epso_write_read(serial, 0x00, 0xB0, 62, dataBuf, BUF_SIZE);
	
	uint64_t cardNum, pinNum;
	char     cardStr[17];
	char     pinMd5Str[33];
	char     waitCounter, state;
	
	waitCounter = cardNum = cardStr[0] = pinNum = pinMd5Str[0] = 0;
	state = WAIT_FOR_FIRST_ID;
	
	#ifdef KLUCZE
	uint64_t card2Num, pin2Num;
	char     card2Str[17];
	card2Num = card2Str[0] = 0;
	#endif
	
	while(1) {
		usleep(100000);
		
		if (state == WAIT_FOR_FIRST_ID) {
			char isNewData = readCardPin2(serial, dataBuf, BUF_SIZE, &cardNum, &pinNum, cardStr, pinMd5Str);
			
			if (isNewData) {
				#ifdef CARD_AND_PIN
				if (isNewData & 0x01) {
					if (state == HAS_PIN) {
						state = HAS_FIRST_ID;
					} else {
						state = HAS_CARD;
						waitCounter++;
					}
				} else if (isNewData & 0x02) {
					if (state == HAS_CARD) {
						state = HAS_FIRST_ID;
					} else {
						state = HAS_PIN;
						waitCounter++;
					}
				}
				#endif
				#ifdef ONLY_CARD
				if (isNewData & 0x01) {
					state = HAS_FIRST_ID;
				} else {
					state = WAIT_FOR_FIRST_ID;
					continue;
				}
				#endif
				#ifdef CARD_OR_PIN
				state = HAS_FIRST_ID;
				#endif
			} else if (waitCounter > 0) { // is waiting for new data
				if (waitCounter++ > 25) {
					epso_write_read(serial, 0x00, 0xB1, 62, dataBuf, BUF_SIZE);
					LPRINT("TIMEOUT card=0x%llx / pin=%lld\n", cardNum, pinNum);
					sleep(1);
					epso_write_read(serial, 0x00, 0xB0, 62, dataBuf, BUF_SIZE);
					
					waitCounter = 0;
					state = WAIT_FOR_FIRST_ID;
				}
				continue;
			}
		}
		
		#ifdef KLUCZE
		if (state == HAS_FIRST_ID) {
			/// TODO: sygnalizacja ???
			char isNewData = readCardPin2(serial, dataBuf, BUF_SIZE, &card2Num, &pin2Num, card2Str, NULL);
			if (isNewData) {
				if (isNewData & 0x01) {
					state = HAS_SECOND_ID;
				}
			} else if (waitCounter > 0) { // is waiting for new data
				if (waitCounter++ > 250) { // is waiting timeout
					epso_write_read(serial, 0x00, 0xB1, 62, dataBuf, BUF_SIZE);
					LPRINT("TIMEOUT card=0x%llx / pin=%lld\n", cardNum, pinNum);
					sleep(1);
					epso_write_read(serial, 0x00, 0xB0, 62, dataBuf, BUF_SIZE);
					
					waitCounter = 0;
					state = WAIT_FOR_FIRST_ID;
				}
				continue;
			}
		}
		if (state == HAS_SECOND_ID) {
			DPRINT("SEARCH IN DB: card = %llx (%s) / pin = %llu (%s) FOR ZONE: %llx (%s) \n", cardNum, cardStr, pinNum, pinMd5Str, card2Num, card2Str);
			
			snprintf(dataBuf, 500,
				"SELECT distinct users.name, users.card, users.pin, doors.name, users.expire_date FROM users JOIN user_zone JOIN zones JOIN doors JOIN zone_door JOIN keys ON ( "
					"users.userid = user_zone.userid AND user_zone.zoneid = zones.zoneid AND doors.doorid = zone_door.doorid AND zone_door.zoneid = zones.zoneid AND doors.name = keys.name AND "
					"users.card = '%s' AND keys.id = '%s');", cardStr, card2Str
			);
			
			DPRINT("SQL: %s\n", dataBuf);
			
			int ret;
			sqlite3_stmt *stmt;
			ret = sqlite3_prepare_v2( db, dataBuf, -1, &stmt, NULL );
			
			const char *userName = NULL;
			const char *door = NULL;
			int expire = 0;
			while ( ( ret = sqlite3_step(stmt) ) == SQLITE_ROW ) {
				userName = (const char*) sqlite3_column_text(stmt, 0);
				door = (const char*) sqlite3_column_text(stmt, 3);
				expire = sqlite3_column_int(stmt, 4);
				if (time(0) > expire)
					state = HAS_USERNAME;
				break;
			}
			
			if (state == HAS_USERNAME) {
				epso_write_read(serial, 0x00, 0xe8, GREEN, dataBuf, BUF_SIZE);
				LPRINT("ACCESS FOR %s GRANTED FOR: %s (0x%s, %lld)\n", door, userName, cardStr, pinNum);
				sendEvent(door, "access_granted", cardStr, userName);
				sleep(3);
				epso_write_read(serial, 0x00, 0xe8, ORANGE, dataBuf, BUF_SIZE);
			} else {
				epso_write_read(serial, 0x00, 0xe8, RED, dataBuf, BUF_SIZE);
				epso_write_read(serial, 0x00, 0xB1, 62, dataBuf, BUF_SIZE);
				LPRINT("ACCESS DENIED card=0x%s / pin=%lld\n", cardStr, pinNum);
				sendEvent(door, "access_denied", cardStr, NULL);
				sleep(1);
				epso_write_read(serial, 0x00, 0xB0, 62, dataBuf, BUF_SIZE);
				epso_write_read(serial, 0x00, 0xe8, ORANGE, dataBuf, BUF_SIZE);
			}
			waitCounter = cardNum = cardStr[0] = pinNum = pinMd5Str[0] = card2Num = card2Str[0] = 0;
			state = WAIT_FOR_FIRST_ID;
		}
		#else // KLUCZE
		if (state == HAS_FIRST_ID) {
			DPRINT("SEARCH IN DB: card = %llx (%s) / pin = %llu (%s)\n", cardNum, cardStr, pinNum, pinMd5Str);
			
			snprintf(dataBuf, BUF_SIZE,
				"SELECT distinct users.name, users.card, users.pin, doors.name, users.expire_date FROM users JOIN user_zone JOIN zones JOIN doors JOIN zone_door ON ( "
					"users.userid = user_zone.userid AND user_zone.zoneid = zones.zoneid AND doors.doorid = zone_door.doorid AND zone_door.zoneid = zones.zoneid AND "
#ifdef WINDA
					"doors.name LIKE '%s%%' AND "
#else
					"doors.name = '%s' AND "
#endif
					#ifdef CARD_AND_PIN
					"(users.card = '%s' AND users.pin = '%s') );", argv[1], cardStr, pinMd5Str
					#endif
					#ifdef ONLY_CARD
					"users.card = '%s' );", argv[1], cardStr
					#endif
					#ifdef CARD_OR_PIN
					"AND (users.card = '%s' OR users.pin = '%s') );", argv[1], cardStr, pinMd5Str
					#endif
			);
			
			DPRINT("SQL: %s\n", dataBuf);
			
			int ret;
			sqlite3_stmt *stmt;
			ret = sqlite3_prepare_v2( db, dataBuf, -1, &stmt, NULL );
			
			const char *userName = NULL;
			int expire = 0;
			#ifdef WINDA
			char pietra[4] = {0,0,0,0};
			#endif
			
			while ( ( ret = sqlite3_step(stmt) ) == SQLITE_ROW ) {
				userName = (const char*) sqlite3_column_text(stmt, 0);
				expire = sqlite3_column_int(stmt, 4);
				int curr_time = time(0);
				DPRINT("FIND IN DB: %s -> %d vs %d\n", userName, expire, curr_time);
				if (curr_time < expire)
					state = HAS_USERNAME;
				else
					break;
				
				#ifdef WINDA
				const char* door = (const char*) sqlite3_column_text(stmt, 3);
				int pietro = atoi(door+12);
				DPRINT("FIND IN DB: %s -> %d \n", door, pietro);
				pietra[pietro]=1;
				#else
				break;
				#endif
			}
			
			if (state == HAS_USERNAME) {
				
				#ifdef WINDA
				if (pietra[0]) system ("npe +DO2");
				if (pietra[1]) system ("npe +DO1");
				if (pietra[2]) system ("npe +PO2");
				if (pietra[3]) system ("npe +PO1");
				#else
				system ("npe +PO1");
				#endif
				epso_write_read(serial, 0x00, 0xe8, GREEN, dataBuf, BUF_SIZE);
				LPRINT("ACCESS FOR %s GRANTED FOR %s (0x%s, %lld)\n", argv[1], userName, cardStr, pinNum);
				sendEvent(argv[1], "access_granted", cardStr, userName);
				sleep(3);
				epso_write_read(serial, 0x00, 0xe8, ORANGE, dataBuf, BUF_SIZE);
			} else {
				
				epso_write_read(serial, 0x00, 0xe8, RED, dataBuf, BUF_SIZE);
				epso_write_read(serial, 0x00, 0xB1, 62, dataBuf, BUF_SIZE);
				LPRINT("ACCESS FOR %s DENIED card=0x%s / pin=%lld\n", argv[1], cardStr, pinNum);
				sendEvent(argv[1], "access_denied", cardStr, NULL);
				sleep(1);
				epso_write_read(serial, 0x00, 0xB0, 62, dataBuf, BUF_SIZE);
				epso_write_read(serial, 0x00, 0xe8, ORANGE, dataBuf, BUF_SIZE);
			}
			
			system ("npe -DO1; npe -DO2; npe -PO1; npe -PO2");
			waitCounter = cardNum = cardStr[0] = pinNum = pinMd5Str[0] = 0;
			state = WAIT_FOR_FIRST_ID;
		}
		#endif // KLUCZE
	}
	return 0;
}
