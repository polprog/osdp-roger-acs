#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "error_reporting.h"
#include "readers/reader.h"

#include "readers/epso/epso.h"

#define LED_CMD     0xe8

#define LED_RED     0x4
#define LED_GREEN   0xa
#define LED_ORANGE  0xd
#define LED_DEFAULT 0x0 // 0x1

#define BEEP_ON     0xB1
#define BEEP_OFF    0xB0
#define BEEP_LEN    62 // 62 * 0.125s

#define BUF_SIZE 128

const uint8_t defaultAddress = 0x00;

struct Reader {
	int serial;
	uint8_t address;
	void* user_data;
};

Reader* init_reader(const char* readerDevice, bool use_rs485_te, uint8_t readerAddress, void* readerUserData) {
	int readerSerial = init_tty(readerDevice, B9600);
	if ( readerSerial < 0 ) {
		LOG_PRINT_CRIT("Open TTY ERROR\n");
		return 0;
	}
	
	Reader* reader  = malloc(sizeof (struct Reader));
	reader->serial  = readerSerial;
	reader->address = readerAddress;
	
	reader->user_data = readerUserData;

	epso_write_read(readerSerial, reader->address, BEEP_ON,  BEEP_LEN, 0, 0);
	epso_write_read(readerSerial, reader->address, LED_CMD,  LED_DEFAULT, 0, 0);
	epso_write_read(readerSerial, reader->address, BEEP_OFF, BEEP_LEN, 0, 0);
	
	return reader;
}

void get_reader_data(Reader* reader, int input_timeout, void* user_data) {
	char dataBuf[ BUF_SIZE ];
	uint64_t cardNum, pinNum;
	char isNewData = readCardPin(reader->serial, reader->address, dataBuf, BUF_SIZE, &cardNum, &pinNum, NULL, NULL);
	
	char dataLen = 0;
	if (isNewData & 0x01) {
		if (cardNum & 0x00ffffff00000000LL) {
			dataLen = 7;
		} else {
			dataLen = 4;
		}
		process_user_info((uint8_t *)(&cardNum), dataLen, DATA_IS_CARD, DATA_OK, reader, reader->user_data, user_data);
	} else  if (isNewData & 0x02) {
		dataLen = sprintf(dataBuf, "%lld", (long long)pinNum);
		process_user_info((uint8_t *)(dataBuf), dataLen, DATA_IS_PIN, DATA_OK, reader, reader->user_data, user_data);
	}
}

void clear_reader_data(Reader* reader) {
}

void reader_signal(char signal, Reader* reader) {
	switch(signal){
		case SIGNAL_OK:
			epso_write_read(reader->serial, reader->address, LED_CMD,  LED_GREEN, 0, 0);
			sleep(3);
			epso_write_read(reader->serial, reader->address, LED_CMD,  LED_DEFAULT, 0, 0);
			break;
		case SIGNAL_DOOR_OPEN:
			epso_write_read(reader->serial, reader->address, LED_CMD,  LED_GREEN, 0, 0);
			break;
		case SIGNAL_DOOR_LOCK:
			epso_write_read(reader->serial, reader->address, LED_CMD,  LED_DEFAULT, 0, 0);
			break;
		case SIGNAL_ERR:
		case SIGNAL_ERR_AUTH:
		case SIGNAL_ERR_COMM:
			epso_write_read(reader->serial, reader->address, LED_CMD,  LED_RED, 0, 0);
			epso_write_read(reader->serial, reader->address, BEEP_ON,  BEEP_LEN, 0, 0);
			sleep(1);
			epso_write_read(reader->serial, reader->address, BEEP_OFF, BEEP_LEN, 0, 0);
			epso_write_read(reader->serial, reader->address, LED_CMD,  LED_DEFAULT, 0, 0);
			break;
		case SIGNAL_ERR_USAGE:
			epso_write_read(reader->serial, reader->address, BEEP_ON,  BEEP_LEN, 0, 0);
			sleep(1);
			epso_write_read(reader->serial, reader->address, BEEP_OFF, BEEP_LEN, 0, 0);
			break;
		case SIGNAL_PIN:
			epso_write_read(reader->serial, reader->address, LED_CMD, LED_ORANGE, 0, 0);
			break;
		case SIGNAL_PIN_QUIET:
			break;
	}
}
