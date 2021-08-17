#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "md5.h"

#include "error_reporting.h"
#include "eventSend.h"
#include "user_db.h"

#define MD5LEN      32+1

#define BUF_SIZE 500

#define CHECK_BUFFER_OVERFLOW(size) if (size >= BUF_SIZE) { LOG_PRINT_ERROR("SQL query buffer overflow"); return 0; }

int32_t getAccessMask2(const uint8_t* pin, int pinLen, const uint8_t* card, int cardLen, uint8_t mode, const char* doorname, sqlite3 *db) {
	char dataBuf[ BUF_SIZE ];
	char pin_md5[MD5LEN];
	char cardStr[MAX_CARD_LEN*2+1];
	
	if (mode == MODE_PIN || mode == MODE_CARD_AND_PIN_ALWAYS) {
		unsigned char digest[16];
		MD5_CTX context;
		MD5_Init(&context);
		MD5_Update(&context, pin, pinLen);
		MD5_Final(digest, &context);
		
		for(int i = 0; i < 16; ++i) {
			sprintf(pin_md5 + i*2, "%02x", (unsigned int)digest[i]);
		}
		pin_md5[32] = 0;
		DPRINT("Wprowadzono pin %s\n", pin);
		DPRINT("Wprowadzono md5 %s\n", pin_md5);
	} else {
		pin_md5[0] = 0;
	}
	
	if (mode == MODE_CARD || mode == MODE_CARD_AND_PIN || mode == MODE_CARD_AND_PIN_ALWAYS) {
		if (cardLen > MAX_CARD_LEN) {
			LOG_PRINT_ERROR("call getAccessMask with invalid cardLen %d", cardLen);
			return 0;
		}
		for(int i = 0; i < cardLen; ++i) {
			sprintf(cardStr + i*2, "%02X", card[i]);
		}
	} else {
		cardStr[0] = 0;
	}
	
	int ret;
	ret = snprintf(dataBuf, BUF_SIZE,
		"SELECT DISTINCT users.name, users.pin, users.expire_date, user_door.allow_no_pin, user_door.time_expr, doors.name FROM users JOIN user_door JOIN doors ON ( "
		"users.userid = user_door.userid AND user_door.doorid = doors.doorid AND doors.name LIKE '%s' AND ", doorname
	);
	CHECK_BUFFER_OVERFLOW(ret);
	
	switch (mode) {
		case MODE_PIN:
			ret += snprintf(dataBuf + ret, BUF_SIZE - ret, "users.pin == '%s'", pin_md5);
			break;
		case MODE_CARD_AND_PIN:
		case MODE_CARD_AND_PIN_ALWAYS:
		//	ret += snprintf(dataBuf + ret, BUF_SIZE - ret, "users.pin == '%s' AND users.card == '%s'", pin_md5, cardStr);
		//	break;
		case MODE_CARD:
			ret += snprintf(dataBuf + ret, BUF_SIZE - ret, "users.card == '%s'", cardStr);
			break;
		default:
			LOG_PRINT_ERROR("call getAccessMask with invalid mode %d", mode);
			return 0;
	}
	CHECK_BUFFER_OVERFLOW(ret);
	
	ret += snprintf(dataBuf + ret, BUF_SIZE - ret, ");");
	CHECK_BUFFER_OVERFLOW(ret);
	
	DPRINT("SQL: %s\n", dataBuf);
	
	sqlite3_stmt *stmt;
	ret = sqlite3_prepare_v2( db, dataBuf, -1, &stmt, NULL );
	const char *userName = NULL;
	const char *userPin = NULL;
	const char *timeExpr = NULL;
	int expire = 0;
	int allow_no_pin = 0;
	int errType = EVENT_AUTH_ERR;
	uint32_t door_mask = 0;
	
	while ( ( ret = sqlite3_step(stmt) ) == SQLITE_ROW ) {
		userName = (const char*) sqlite3_column_text(stmt, 0);
		userPin  = (const char*) sqlite3_column_text(stmt, 1);
		expire        = sqlite3_column_int(stmt, 2);
		allow_no_pin  = sqlite3_column_int(stmt, 3);
		timeExpr = (const char*) sqlite3_column_text(stmt, 4);
		int curr_time = time(0);
		DPRINT("FIND IN DB: %s -> %d vs %d ... ", userName, expire, curr_time);
		errType = EVENT_AUTH_ERR;
		if (curr_time > expire) {
			errType = EVENT_AUTH_ERR_EXPIRE;
			continue;
		}
		if (mode == MODE_CARD_AND_PIN_ALWAYS && strcmp(userPin, pin_md5) != 0) {
			errType = EVENT_AUTH_ERR_PIN;
			continue;
		}
		if (mode == MODE_CARD_AND_PIN && strcmp(userPin, pin_md5) != 0 && allow_no_pin != 1) {
			errType = EVENT_AUTH_ERR_PIN;
			continue;
		}
		/* TODO verify time expression ("timetables")
		 *  - expression timeExpr with:
		 *    wd = week day
		 *    dmin = day minutes (from midnight)
		 *    tzmin = time zone offest in minutes (midnight ofset from 00:00 UTC)
		 *    mm = month
		 *    dm = day of month
		 *    unixtimestamp = seconds from 1970-01-01 00:00:00 UTC
		 *  - replace this string by current values and calculate expression logic value
		 *  - empty expression == all time access
		 */
		char* door = (char*) sqlite3_column_text(stmt, 5);
		char* percent = strstr(doorname, "%");
		int subdoor = 0;
		if(percent != NULL){
			subdoor = atoi(door+(percent-doorname));
		}
		DPRINT("access for door=%s subdoor=%d \n", door, subdoor);
		door_mask |= (1 << subdoor);
	}
	
	if (door_mask) {
		sendEvent(EVENT_AUTH_OK, doorname, door_mask, userName, cardStr);
	} else {
		sendEvent(errType, doorname, userName, cardStr);
	}
	
	return door_mask;
}

int32_t checkKeyAccess2(const uint8_t* card1, int card1Len, const uint8_t* card2, int card2Len, sqlite3 *db) {
	char dataBuf[ BUF_SIZE ];
	char card1Str[MAX_CARD_LEN*2+1];
	char card2Str[MAX_CARD_LEN*2+1];
	if (card1Len > MAX_CARD_LEN || card2Len > MAX_CARD_LEN) {
		LOG_PRINT_ERROR("call checkKeyAccess with invalid cardLen %d %d", card1Len, card2Len);
		return 0;
	}
	for(int i = 0; i < card1Len; ++i) {
		sprintf(card1Str + i*2, "%02X", card1[i]);
	}
	for(int i = 0; i < card2Len; ++i) {
		sprintf(card2Str + i*2, "%02X", card2[i]);
	}
	
	int ret;
	ret = snprintf(dataBuf, BUF_SIZE,
		"SELECT DISTINCT users.name, users.expire_date, doors.name FROM users JOIN user_door JOIN doors JOIN keys ON ( "
		"users.userid = user_door.userid AND AND user_door = doors.doorid AND doors.doorid == keys.doorid "
		"users.card = '%s' AND keys.cardid = '%s');", card1Str, card2Str
	);
	CHECK_BUFFER_OVERFLOW(ret);
	
	DPRINT("SQL: %s\n", dataBuf);
	
	sqlite3_stmt *stmt;
	ret = sqlite3_prepare_v2( db, dataBuf, -1, &stmt, NULL );
	
	const char *userName = NULL;
	const char *door = NULL;
	int expire = 0;
	uint32_t door_mask = 0;
	
	while ( ( ret = sqlite3_step(stmt) ) == SQLITE_ROW ) {
		userName = (const char*) sqlite3_column_text(stmt, 0);
		expire   = sqlite3_column_int(stmt, 1);
		door     = (const char*) sqlite3_column_text(stmt, 2);
		if (time(0) < expire) {
			door_mask = 1;
		}
		break;
	}
	
	if (door_mask) {
		sendEvent(EVENT_AUTH_OK, door, door_mask, userName, card1Str);
	} else {
		sendEvent(EVENT_AUTH_ERR, card2Str, userName, card1Str);
	}
	
	return door_mask;
}
