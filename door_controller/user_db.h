#include <stdint.h>
#include <sqlite3.h>

#include "door_controller.h"

enum {
	MODE_CARD                = 0x01,
	MODE_PIN                 = 0x02,
	MODE_CARD_OR_PIN         = 0x10,
	MODE_CARD_AND_PIN        = 0x20,
	MODE_CARD_AND_PIN_ALWAYS = 0x30,
	MODE_TWO_CARDS           = 0x40,
};

uint32_t getAccessMask2(const uint8_t* pin, int pinLen, const uint8_t* card, int cardLen, uint8_t mode, const char* doorname, sqlite3 *db);

uint32_t checkKeyAccess2(const uint8_t* card1, int card1Len, const uint8_t* card2, int card2Len, sqlite3 *db);

static inline uint32_t getAccessMask(const DoorReader* doorReader, uint8_t mode, const char* doorname, sqlite3 *db) {
	return getAccessMask2(doorReader->pin, doorReader->pinLen, doorReader->card, doorReader->cardLen, mode, doorname, db);
}

static inline uint32_t checkKeyAccess(const DoorReader* doorReader, sqlite3 *db) {
	return checkKeyAccess2(doorReader->card, doorReader->cardLen, doorReader->card2, doorReader->card2Len, db);
}
