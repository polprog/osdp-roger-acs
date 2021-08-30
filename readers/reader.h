#ifndef READER_H
#define READER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * reader handling functions
 */

struct Reader;
typedef struct Reader Reader;

Reader* init_reader(const char* readerDevice, bool use_rs485_te, uint8_t readerAddress, void* readerUserData);

void close_reader(Reader* reader);

void get_reader_data(Reader* reader, int input_timeout, void* user_data); // call process_user_info()

void clear_reader_data(Reader* reader);

enum {
 SIGNAL_OK        = 0x10,
 SIGNAL_DOOR_OPEN = 0x11,
 SIGNAL_DOOR_LOCK = 0x12,
 SIGNAL_ERR       = 0x20,
 SIGNAL_ERR_AUTH  = 0x21,
 SIGNAL_ERR_USAGE = 0x22,
 SIGNAL_ERR_COMM  = 0x23,
 SIGNAL_PIN       = 0x41,
 SIGNAL_PIN_QUIET = 0x42,
 SIGNAL_BLOCKED   = 0x81,
 SIGNAL_UNBLOCKED = 0x82,
};

void reader_signal(char signal, Reader* reader);

extern const uint8_t defaultAddress;

/**
 * calback functions for readers
 */

enum DataType {
 DATA_IS_PIN  = 1,
 DATA_IS_CARD = 2,
};

enum DataMode {
 DATA_OK         = 1,
 DATA_OUT_OF_BUF = 2,
 DATA_TIMEOUT    = 3,
};

/*
when dataType == DATA_IS_PIN:
	data is NULL end string with text (ASCII) representation of pin
	dataLen is string len of data
when dataType == DATA_IS_CARD:
	data is byte array of card number
	dataLen is length of data array
*/
void process_user_info(uint8_t *data, int dataLen, enum DataType dataType, enum DataMode dataMode, Reader* reader, void* readerUserData, void* userData);

#endif
