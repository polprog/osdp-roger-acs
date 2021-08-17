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
	// mask GPIO outputs for all subdoors (0x01 for standard doors, e.g. 0x0f for 4 levels elevator)
	// maskOffset is aplay to maskFull
	uint8_t     maskFull;
	
	int         door_lock_timer;
};
typedef struct Door Door;

void mainLoop(Door* doors, int doorsCount, sqlite3 *database);

#endif
