#ifndef DOOR_CONTROLER_H
#define DOOR_CONTROLER_H

#include <sqlite3.h>
#include "readers/reader.h"

#define MAX_CARD_LEN 16
#define MAX_PIN_LEN  16

struct DoorReader {
	Reader*     reader;
	uint8_t     accessMode;
	int         two_factor_timer;
	
	int         cardLen;
	int         card2Len;
	int         pinLen;
	
	uint8_t     card[MAX_CARD_LEN];
	uint8_t     card2[MAX_CARD_LEN];
	uint8_t     pin[MAX_PIN_LEN + 1];
};
typedef struct DoorReader DoorReader;

struct Door {
	const char* doorName;
	
	//
	// readers
	//
	
	DoorReader  readerA;
	DoorReader  readerB;
	
	//
	// inputs
	//
	
	uint8_t     lastInput;
	int         alarm_timers;
	
	//
	// outputs
	//
	
	// offset of this doors in GPIO outputs
	uint8_t     maskOffset;
	// mask GPIO outputs for all subdoors (0x01 for standard doors, e.g. 0x0f for 4 levels elevator), maskOffset is apply to maskFull
	uint32_t    maskFull;
	// when >0 door are unlock and door_lock_timer contains number of main loop cycles until door will be locked
	int         door_lock_timer;
	
	//
	// blocking and airlock
	//
	
	// administrative block change door state (the door will remain in current state – locked or unlocked)
	uint32_t    admDisableMask;
	// temporary block mask for unlock operation, used for airlock mode (set externally by partner)
	uint32_t    airLockMask;
	// door partner for airlock mode, used to set lock on it when our door is unlock or open, NULL when not airlock mode
	const char*      airLockPartnerString;
	struct Door*     airLockPartnerLocal;
	struct addrinfo* airLockPartnerNet;
};
typedef struct Door Door;

Door* getDoorByName(Door* doors, int doorsCount, const char* name);
void mainLoop(Door* doors, int doorsCount, sqlite3 *database);
uint32_t unlock_door(Door* door, uint32_t mask);
uint32_t lock_door(Door* door, uint32_t mask);
void signalDoorStatus(Door* door);

#endif
