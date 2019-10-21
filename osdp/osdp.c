#include "osdp.h"




bool process_packet(struct osdp_packet *packet, struct osdp_response *response){
	//printf("Processing... pdata=0x%02x\n", packet->data[5]);
	//correct length
	packet->len = (packet->data[3] << 8) | packet->data[2];
	//check crc
	uint16_t crc = fCrcBlk(packet->data, packet->len-2);

	if(crc != (uint16_t) ((packet->data[packet->len-1]<<8) | 
			packet->data[packet->len-2])) {
		printf("CRC: expected=%04x, got %04x\n", crc, 
			(packet->data[packet->len-1]<<8) | 
			packet->data[packet->len-2]);
		fprintf(stderr, "CRC not OK\n");
		return false;
	}
	
	response->payloadlen = 0;
	response->response = packet->data[5];
	switch(packet->data[5]){
		case osdp_ACK:
			printf("Acknowledge\n");
			break;
		case osdp_NACK:
			printf("Not Acknowledge\n");
			break;
		case osdp_RAW:
			printf("Raw card data");
			for(int i = 6; i < packet->len-2; i++){
				if((i-6) % 8 == 0) printf("\n%08x: ", i-6);
				printf("%02x ", packet->data[i]);
				
			}
			printf("\n");
			memcpy(response->payload, packet->data+6, packet->len-8);
			response->payloadlen = packet->len-8;

			break;
		case osdp_KPD:
			printf("Keypad data");
			packet_dump(packet);
			uint16_t pinlen = packet->data[7];
			printf("PIN len=%d\n", pinlen);
				
			for(int i = 8; i <pinlen+8; i++){
				printf("%d ", packet->data[i] & 0x0f);
			}
			printf("\n------");
			for(int i = 6; i < packet->len-2; i++){
				if((i-6) % 8 == 0) printf("\n%08x: ", i-6);
				printf("%02x ", packet->data[i]);
			}
			memcpy(response->payload, packet->data+8, pinlen);
			response->payloadlen = pinlen;
			printf("\n");
			break;
		case osdp_COM:
			printf("COM data");
			memcpy(response->payload, packet->data+6, packet->len-8);
			response->payloadlen = packet->len-8;
			packet_dump(packet);
			break;
		default:
			printf("Unknown packet\n");
			packet_dump(packet);
			return false;
	}
	return true;
}

bool send_packet(struct osdp_packet *packet, int fd){
	if(write(fd, packet->data, packet->len) < 0){
		perror("Failed to write packet");
		return false;
	}
	return true;
}

bool recv_packet(struct osdp_packet *packet, int fd){
	int len;
	if((len = read(fd, packet->data, PACKETLEN)) > 0){
		packet->len = len;
		if(packet->data[0] != 0x53){
			fprintf(stderr, "Invalid packet:");
			packet_dump(packet);
			return false;
		}else{
			return true;
		}
	}else if(len==0){
		fprintf(stderr, "read 0 bytes...\n");
	}else{
		perror("read error");
	}
	return false;
}
void packet_dump(struct osdp_packet *packet){
	printf("Packet at %p, len=%d =0x%04x", packet, packet->len, packet->len);
	for(int i = 0; i < packet->len; i++){
		if(i % 8 == 0) printf("\n%08x: ", i);
		printf("%02x ", (uint8_t) packet->data[i]);
	}
	printf("\n");
}
void crclen_packet(struct osdp_packet *packet){
	uint16_t crc;
	packet->len += 2;
  	packet->data[2] = packet->len & 0xff;
	packet->data[3] = packet->len >> 8;
	//uwaga: wczesniej zwiekszamy len ale dwa ostatnie bajty to smieci! musimy przekazac
	//len - 2 aby ich nie brac do sumy
      	crc = fCrcBlk(packet->data, packet->len-2); 
	//printf("CRC: %04x\n", crc);
	packet->data[packet->len-2] = crc & 0xff;
	packet->data[packet->len-1] = crc >> 8;
		
}

// table based CRC - this is the "direct table" mode -
uint16_t fCrcBlk( uint8_t *pData, uint16_t nLength) {
	uint16_t nCrc;
	int ii;
	for ( ii = 0, nCrc = 0x1D0F; ii < nLength; ii++ ) {
		nCrc = (nCrc<<8) ^ CRC16Table[ ((nCrc>>8) ^ pData[ii]) & 0xFF];
	}
	return nCrc;
}


void comset(struct osdp_packet *packet, char newaddress, uint32_t baudrate){
	char payload[5];
	payload[0] = newaddress;
	payload[1] = baudrate;
	fill_packet(packet, 0x7f, osdp_COMSET, payload, 5);
	packet_dump(packet);
}

void ledset(struct osdp_packet *packet, char address, char lednum, char color, bool blink, int blink_time){
	char ledy[] = {0,lednum,0,0,0,0,0,0,0,0,0,0,0,0};
	/* 0 - reader number
	 * 1 - led number
	 * ------- Temporary settings -------
	 * 2 - control code
	 * 3 - on time
	 * 4 - off time
	 * 5 - on color
	 * 6 - off color
	 * 7 - timer LSB
	 * 8 - timer MSB
	 * ------- Permanent settings -------
	 * 9 - control code
	 * 10 - on time
	 * 11 - off time
	 * 12 - on color
	 * 13 - off color 
	 */
	if(blink_time){
		ledy[2] = 2; 
		ledy[3] = 1;
		ledy[4] = 1;
		ledy[5] = color;
		ledy[7] = blink_time;
		if(!blink){
			ledy[6] = color;	
		}
	}else{
		ledy[2] = 1; ledy[9] = 1; ledy[10] = 1; ledy[11] = 1;
		ledy[12] = color;
		if(!blink) ledy[13] = color;
	}
	fill_packet(packet, address, osdp_LED, &ledy, 14);
}

void beepset(struct osdp_packet *packet, char address, char on_time, char off_time, char count){
	char beep[] = {0, 2, on_time, off_time, count}; //tonecode 2 dziala
	fill_packet(packet, address, osdp_BUZ, beep, 5);
}

void fill_packet(struct osdp_packet *packet, char address, char command, void* data, int datalen){
	packet->data[0] = 0x53;
	packet->data[1] = address;
	packet->data[2] = 0x0; /* LEN MSB */
	packet->data[3] = 0x0; /* LEN LSB */
	packet->data[4] = 0x04; /* Control -> CRC16 */
	packet->data[5] = command;
	memcpy(&(packet->data[6]), data, datalen);
	packet->len=6+datalen;
	//TODO: add actual data
	crclen_packet(packet);
}

bool portsetup(int portfd, struct termios *options){
	int status;
	struct serial_rs485 rs485;
	// Set the serial port in 485 mode
	rs485.flags = (SER_RS485_ENABLED | SER_RS485_RTS_AFTER_SEND);
	rs485.flags &= ~(SER_RS485_RTS_ON_SEND);
	rs485.delay_rts_after_send = 0; 
	rs485.delay_rts_before_send = 0; 
#ifndef _USE_RS232
	status = ioctl(portfd, TIOCSRS485, &rs485);  
	if(status) {
		perror("Failed to set up RS485 (ioctl error)");
	}
#endif //_USE_RS232
	tcgetattr(portfd, options);
	cfsetispeed(options, B9600);
	cfsetospeed(options, B9600);

	options->c_cflag &= ~PARENB;
	options->c_cflag &= ~CSTOPB;
	options->c_cflag &= ~CSIZE; /* Mask the character size bits */
	options->c_cflag |= CS8;    /* Select 8 data bits */
	options->c_cflag |= (CLOCAL | CREAD);
	//options->c_cflag |= IGNBRK;
	//options->c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IXOFF); //raw input
	options->c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG ); //raw input
	//fcntl(portfd, F_SETFL, FNDELAY); //enable blocking
	cfmakeraw(options);
	tcsetattr(portfd, TCSANOW, options);
	tcflush(portfd, TCIOFLUSH);
	return true;
}



