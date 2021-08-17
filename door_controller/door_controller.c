#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "error_reporting.h"

#include "door_controller.h"
#include "user_db.h"
#include "eventSend.h"

#include "readers/reader.h"
#include "gpios/gpio.h"

#ifndef DOOR_OPEN_TIME
#define DOOR_OPEN_TIME  13
#endif

#ifndef TWO_FACTOR_TIME
#define TWO_FACTOR_TIME 23
#endif

#ifndef MAIN_LOOP_SLEEP_TIME
#define MAIN_LOOP_SLEEP_TIME 300000
#endif

#ifndef ALARM_RESEND_TIMER
#define ALARM_RESEND_TIMER 60
#endif

#define ALARM_MASK 1<<3 // disable alarm on missing DI_IS_DOOR_LOCK signal


void door_signal(Door* door, char signal) {
	if (door->readerA.reader)
		reader_signal(signal, door->readerA.reader);
	if (door->readerB.reader)
		reader_signal(signal, door->readerB.reader);
}

void resetDoorAuth(DoorReader* doorReader) {
	doorReader->cardLen = 0;
	doorReader->pinLen = 0;
	doorReader->two_factor_timer = 0;
}

void unlock_door(Door* door, int32_t mask) {
	mask = (mask & (door->maskFull)) << (door->maskOffset);
	set_door_state(DO_UNLOCK, mask);
	sendEvent(EVENT_DOOR_UNLOCK, door->doorName, mask);
	door->door_lock_timer = DOOR_OPEN_TIME;
	door_signal(door, SIGNAL_DOOR_OPEN);
}

void lock_door(Door* door, int32_t mask) {
	mask = (mask & (door->maskFull)) << (door->maskOffset);
	set_door_state(DO_LOCK, mask);
	sendEvent(EVENT_DOOR_LOCK, door->doorName, mask);
	door_signal(door, SIGNAL_DOOR_LOCK);
}

void check_inputs(Door* doors, int doorsCount) {
	uint8_t inputVal[doorsCount];
	get_input_state(inputVal, ~(~0x00 << doorsCount));
	
	for (int i=0; i<doorsCount; ++i) {
		Door* door = &(doors[i]);
		uint8_t input = inputVal[i];
		uint8_t diff;
		if (door->lastInput & DI_STATE_IS_DIFF) {
			diff = door->lastInput;
		} else {
			diff = input ^ door->lastInput;
		}
		if (diff) {
			if (diff & DI_MANUAL_OPEN) {
				if (input & DI_MANUAL_OPEN) {
					sendEvent(EVENT_MANUAL_OPEN, door->doorName);
					unlock_door(door, 1);
				}
			}
			if (diff & DI_IS_EMERGENCY_OPEN) {
				if (input & DI_IS_EMERGENCY_OPEN) {
					sendEvent(EVENT_DOOR_EMERGENCY_ACTIVE, door->doorName);
				} else {
					sendEvent(EVENT_DOOR_EMERGENCY_INACTIVE, door->doorName);
				}
			}
			if (diff & DI_IS_DOOR_CLOSE) {
				if (input & DI_IS_DOOR_CLOSE) {
					sendEvent(EVENT_DOOR_IS_CLOSE, door->doorName);
				} else {
					sendEvent(EVENT_DOOR_IS_OPEN, door->doorName);
				}
			}
			if (diff & DI_IS_DOOR_LOCK) {
				if (input & DI_IS_DOOR_LOCK) {
					sendEvent(EVENT_DOOR_IS_LOCK, door->doorName);
				} else {
					sendEvent(EVENT_DOOR_IS_UNLOCK, door->doorName);
				}
			}
			door->lastInput = input;
		}
		
		uint8_t alarm = ((input | ALARM_MASK) & (DI_IS_EMERGENCY_OPEN | DI_IS_DOOR_CLOSE | DI_IS_DOOR_LOCK)) ^ (DI_IS_DOOR_CLOSE | DI_IS_DOOR_LOCK);
		if (alarm) {
			if (door->alarm_timers == 0) {
				door->alarm_timers = ALARM_RESEND_TIMER;
			} else if (--door->alarm_timers == 0) {
				sendEvent(EVENT_DOOR_OPEN_ALARM, door->doorName, alarm);
				door->alarm_timers = ALARM_RESEND_TIMER;
			}
		} else {
			door->alarm_timers = 0;
		}
	}
}

void invalidUsage(Reader* reader, const char* info) {
	LOG_PRINT_INFO("Invalid data (%s) from reader input", info);
	reader_signal(SIGNAL_ERR_USAGE, reader);
}

void authError(Reader* reader, const char* info) {
	LOG_PRINT_WARN("Invalid credential on %s", info);
	reader_signal(SIGNAL_ERR_AUTH, reader);
}

void invalidData(Reader* reader, const char* info) {
	LOG_PRINT_ERROR("Invalid data (%s) from reader input", info);
	reader_signal(SIGNAL_ERR_COMM, reader);
}

void two_factor_timer(DoorReader* doorReader) {
	if (doorReader->two_factor_timer > 0) {
		if (--(doorReader->two_factor_timer) <= 0) {
			resetDoorAuth(doorReader);
			clear_reader_data(doorReader->reader);
			invalidUsage(doorReader->reader, "timeout from two factor");
		} else {
			reader_signal(SIGNAL_PIN_QUIET, doorReader->reader);
		}
	}
}

void process_user_info(uint8_t *data, int dataLen, enum DataType dataType, enum DataMode dataMode, Reader* reader, void* readerUserData, void* userData) {
	Door* door = (Door*)(readerUserData);
	sqlite3* database = (sqlite3*)(userData);
	DoorReader* doorReader;
	if (reader == door->readerA.reader)
		doorReader = &(door->readerA);
	else if (reader == door->readerB.reader)
		doorReader = &(door->readerB);
	else
		return invalidData(reader, "reader inconsistent whit doors");
	
	if (dataMode == DATA_OK) {
		if (dataType == DATA_IS_PIN) {
			if (dataLen > MAX_PIN_LEN) {
				resetDoorAuth(doorReader);
				return invalidData(reader, "pin too long");
			}
			if (doorReader->pinLen) {
				resetDoorAuth(doorReader);
				return invalidUsage(reader, "pin data when pin is set");
			} else {
				memcpy(doorReader->pin, data, dataLen);
				doorReader->pin[dataLen] = 0;
				doorReader->pinLen = dataLen;
			}
		}
		if (dataType == DATA_IS_CARD) {
			if (dataLen > MAX_CARD_LEN) {
				resetDoorAuth(doorReader);
				return invalidData(reader, "card too long");
			}
			if (doorReader->cardLen) {
				if (doorReader->accessMode != MODE_TWO_CARDS || doorReader->card2Len) {
					resetDoorAuth(doorReader);
					return invalidUsage(reader, "card data when card is set");
				}
				memcpy(doorReader->card2, data, dataLen);
				doorReader->card2Len = dataLen;
			} else {
				memcpy(doorReader->card, data, dataLen);
				doorReader->cardLen = dataLen;
			}
		}
	} else if (dataMode == DATA_OUT_OF_BUF) {
		resetDoorAuth(doorReader);
		return invalidData(reader, "data out of input buffer");
	} else if (dataMode == DATA_TIMEOUT) {
		resetDoorAuth(doorReader);
		return invalidUsage(reader, "timeout from reader pin input");
	}
	
	int32_t auth_mask = 0;
	
	switch (doorReader->accessMode) {
		case MODE_CARD_OR_PIN:
			if (doorReader->cardLen) {
				// check card
				auth_mask = getAccessMask(doorReader, MODE_CARD, door->doorName, database);
			} else if (doorReader->pinLen) {
				// check pin
				auth_mask = getAccessMask(doorReader, MODE_PIN, door->doorName, database);
			}
			break;
		case MODE_CARD:
			if (doorReader->cardLen) {
				// check card
				auth_mask = getAccessMask(doorReader, doorReader->accessMode, door->doorName, database);
			}
			break;
		case MODE_PIN:
			if (doorReader->pinLen) {
				// check pin
				auth_mask = getAccessMask(doorReader, doorReader->accessMode, door->doorName, database);
			}
			break;
		case MODE_CARD_AND_PIN:
		case MODE_CARD_AND_PIN_ALWAYS:
			if (doorReader->cardLen && doorReader->pinLen) {
				// check card AND pin
				auth_mask = getAccessMask(doorReader, MODE_CARD_AND_PIN_ALWAYS, door->doorName, database);
				doorReader->two_factor_timer = 0;
				break;
			}
			if (doorReader->cardLen && doorReader->accessMode == MODE_CARD_AND_PIN) {
				// check if this card can open without pin
				auth_mask = getAccessMask(doorReader, MODE_CARD_AND_PIN, door->doorName, database);
				if (auth_mask) {
					doorReader->two_factor_timer = 0;
					break;
				}
				// if not then continue to two factor auth
			}
			if (!doorReader->two_factor_timer) {
				// signal waiting for two factor (pin input, ...)
				reader_signal(SIGNAL_PIN, reader);
				// start counting
				doorReader->two_factor_timer = TWO_FACTOR_TIME;
			}
			break;
		case MODE_TWO_CARDS:
			if (doorReader->cardLen && doorReader->card2Len) {
				// check key access (based on TWO cards)
				auth_mask = checkKeyAccess(doorReader, database);
				doorReader->two_factor_timer = 0;
				doorReader->card2Len = 0;
			} else if (!doorReader->two_factor_timer) {
				// signal waiting for two factor (pin input, ...)
				reader_signal(SIGNAL_PIN, reader);
				// start counting
				doorReader->two_factor_timer = TWO_FACTOR_TIME;
			}
			break;
		default:
			LOG_PRINT_WARN("Call process_user_info with invalid mode %d", doorReader->accessMode);
	}
	
	if (auth_mask) {
		unlock_door(door, auth_mask);
		resetDoorAuth(doorReader);
	} else if (!doorReader->two_factor_timer) {
		authError(reader, door->doorName);
		resetDoorAuth(doorReader);
	}
}

void mainLoop(Door* doors, int doorsCount, sqlite3* database) {
	for (int i=0; i<doorsCount; ++i) {
		Door* door = &(doors[i]);
		
		// close door timer
		if(door->door_lock_timer && --(door->door_lock_timer) <= 0) {
			lock_door(door, door->maskFull);
		}
		
		// two factor timer
		two_factor_timer(&(door->readerA));
		two_factor_timer(&(door->readerB));
		
		// get reader data
		if(door->readerA.reader) get_reader_data(door->readerA.reader, false, database);
		if(door->readerB.reader) get_reader_data(door->readerB.reader, false, database);
	}
	
	check_inputs(doors, doorsCount);
	usleep(MAIN_LOOP_SLEEP_TIME);
}
