#include <termios.h>
#include <stdio.h>
#include <time.h>

#include "error_reporting.h"
#include "readers/reader.h"

#include "readers/osdp/osdp.h"

#define OSDP_SLEEP_TIME 50000
#define MAX_PIN_LEN 28
#define CARD_ID_OFFSET 4

const uint8_t defaultAddress = 0x7f;

struct Reader {
	int serial;
	bool use_rs485_te;
	uint8_t address;
	
	uint8_t pin_buf[MAX_PIN_LEN+1];
	int pin_buf_pos;
	int code_timeout;
	
	void* user_data;
};

Reader* init_reader(const char* readerDevice, bool use_rs485_te, uint8_t readerAddress, void* readerUserData) {
	int readerSerial = portsetup(readerDevice, use_rs485_te);
	if ( readerSerial < 0 ) {
		LOG_PRINT_CRIT("Open TTY ERROR\n");
		return 0;
	}
	
	Reader* reader  = malloc(sizeof (struct Reader));
	reader->serial  = readerSerial;
	reader->address = readerAddress;
	reader->use_rs485_te = use_rs485_te;
	
	reader->pin_buf[0] = 0;
	reader->pin_buf_pos = 0;
	
	reader->user_data = readerUserData;
	
	return reader;
}

void close_reader(Reader* reader) {
	free(reader);
}

void get_reader_data(Reader* reader, int input_timeout, void* user_data) {
	struct osdp_packet packet, answer;
	struct osdp_response res;
	
	fill_packet(&packet, reader->address, osdp_POLL, NULL, 0);
	send_packet(&packet, reader->serial);
	#ifdef OSDP_DEBUG
	packet_dump(&packet);
	#endif
	
	usleep(OSDP_SLEEP_TIME);
	
	if(recv_packet(&answer, reader->serial)) {
		process_packet(&answer, &res);
		#ifdef OSDP_DEBUG
		printf("RES code= %02xh len= %d\n", res.response, res.payloadlen);
		#endif
		switch(res.response){
			case osdp_RAW:
				#ifdef DEBUG
				DPRINT("enter card: 0x");
				for (int i=0; i<res.payloadlen; ++i)
					DPRINT("%02x", res.payload[i]);
				DPRINT("\n");
				#endif
				// card data received
				if (res.payloadlen > CARD_ID_OFFSET) {
					process_user_info(res.payload+CARD_ID_OFFSET, res.payloadlen-CARD_ID_OFFSET, DATA_IS_CARD, DATA_OK, reader, reader->user_data, user_data);
				} else {
					LOG_PRINT_WARN("Card data less than CARD_ID_OFFSET (%d)\n", CARD_ID_OFFSET);
				}
				// break pin entry on card read
				reader->pin_buf_pos = 0;
				break;
			
			case osdp_KPD:
				// received pin data ... complete pin in reader->pin_buf
				for(int i = 0; i < res.payloadlen; i++){
					if(res.payload[i] == 0x7f){ // * on pinpad - clear pin
						reader->pin_buf_pos = 0;
						DPRINT("clear pin\n");
					} else if(res.payload[i] == 0x0d){ // # on pinpad - pin done
						reader->pin_buf[reader->pin_buf_pos] = 0;
						DPRINT("complete pin is: %s\n", reader->pin_buf);
						process_user_info(reader->pin_buf, reader->pin_buf_pos, DATA_IS_PIN, DATA_OK, reader, reader->user_data, user_data);
						reader->pin_buf_pos = 0;
					} else {
						if (reader->pin_buf_pos == MAX_PIN_LEN) { // pin too long
							process_user_info(reader->pin_buf, reader->pin_buf_pos, DATA_IS_PIN, DATA_OUT_OF_BUF, reader, reader->user_data, user_data);
							reader->pin_buf_pos = 0;
						}
						reader->pin_buf[reader->pin_buf_pos] = res.payload[i];
						++reader->pin_buf_pos;
						DPRINT("add %c to pin\n", res.payload[i]);
					}
					reader->code_timeout = input_timeout;
				}
			
			case osdp_ACK:
				if(reader->pin_buf_pos > 0) { // in pin entry mode
					if(reader->code_timeout > 0){
						reader->code_timeout--;
						DPRINT("reader->code_timeout--\n");
					} else if(input_timeout) {
						DPRINT("Code timeout elapsed\n");
						process_user_info(reader->pin_buf, reader->pin_buf_pos, DATA_IS_PIN, DATA_TIMEOUT, reader, reader->user_data, user_data);
						reader->pin_buf_pos = 0;
					}
				}
				break;
			case osdp_NACK:
				LOG_PRINT_WARN("Received NACK from reader\n");
				break;
		}
	} else {
		fprintf(stderr, "Received bad packet\n");
		tcflush(reader->serial, TCIOFLUSH);
		
		char linkPath[24], filePath[24];
		snprintf(linkPath, 24, "/proc/self/fd/%d", reader->serial);
		int ret = readlink(linkPath, filePath, 24);
		if (ret < 23) {
			filePath[ret]=0;
			close(reader->serial);
			reader->serial = portsetup(filePath, reader->use_rs485_te);
			LOG_PRINT_WARN("port %s reopepend and initialized (rs485=%d)", filePath, reader->use_rs485_te);
		}
	}
}

void clear_reader_data(Reader* reader) {
	reader->code_timeout = 0;
	reader->pin_buf_pos = 0;
}

void reader_signal(char signal, Reader* reader) {
	struct osdp_packet packet;
	switch(signal){
		case SIGNAL_OK:
			ledset(&packet, reader->address, 1, GREEN, false, 48);
			send_packet(&packet, reader->serial);
			usleep(OSDP_SLEEP_TIME);
			recv_packet(&packet, reader->serial);
			beepset(&packet, reader->address, 1, 1, 1);
			send_packet(&packet, reader->serial);
			break;
		case SIGNAL_DOOR_OPEN:
			ledset(&packet, reader->address, 1, GREEN, false, 0);
			send_packet(&packet, reader->serial);
			usleep(OSDP_SLEEP_TIME);
			recv_packet(&packet, reader->serial);
			beepset(&packet, reader->address, 1, 1, 1);
			send_packet(&packet, reader->serial);
			break;
		case SIGNAL_DOOR_LOCK:
			ledset(&packet, reader->address, 1, BLACK, false, 0);
			send_packet(&packet, reader->serial);
			break;
		case SIGNAL_ERR:
		case SIGNAL_ERR_AUTH:
		case SIGNAL_ERR_USAGE:
		case SIGNAL_ERR_COMM:
			ledset(&packet, reader->address, 1, RED, true, 24);
			send_packet(&packet, reader->serial);
			usleep(OSDP_SLEEP_TIME);
			recv_packet(&packet, reader->serial);
			beepset(&packet, reader->address, 3, 1, 3);
			send_packet(&packet, reader->serial);
			break;
		case SIGNAL_PIN:
			ledset(&packet, reader->address, 1, AMBER, true, 12);
			send_packet(&packet, reader->serial);
			usleep(OSDP_SLEEP_TIME);
			recv_packet(&packet, reader->serial);
			beepset(&packet, reader->address, 1, 1, 2);
			send_packet(&packet, reader->serial);
			break;
		case SIGNAL_PIN_QUIET:
			ledset(&packet, reader->address, 1, AMBER, true, 12);
			send_packet(&packet, reader->serial);
			break;
		case SIGNAL_BLOCKED:
			ledset(&packet, reader->address, 2, AMBER, true, 0);
			send_packet(&packet, reader->serial);
			usleep(OSDP_SLEEP_TIME);
			recv_packet(&packet, reader->serial);
			ledset(&packet, reader->address, 1, AMBER, true, 0);
			send_packet(&packet, reader->serial);
			break;
		case SIGNAL_UNBLOCKED:
			ledset(&packet, reader->address, 2, BLACK, false, 0);
			send_packet(&packet, reader->serial);
			usleep(OSDP_SLEEP_TIME);
			recv_packet(&packet, reader->serial);
			ledset(&packet, reader->address, 1, BLACK, true, 0);
			send_packet(&packet, reader->serial);
			break;
		default:
			return;
	}
	usleep(OSDP_SLEEP_TIME);
	recv_packet(&packet, reader->serial);
}
