/*
 * comunication with EPSO reader by Roger (eg. PRT12EM)
 * 
 * Copyright (c) 2013-2019 Robert Paciorek,
 * 3-clause BSD license
 */

// #define NO_USE_SYSLOG
// #define DEBUG_OUT

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "roger.h"
#include "error_reporting.h"
#include "md5.h"

#define TTY_READ_TIMEOUT_1 2000000
#define TTY_READ_TIMEOUT_2   40000

#define SOH 0x01
#define STX 0x02
#define ETX 0x03

int init_tty(const char *tty_device, int tty_baud_flag) {
	struct termios term;
	int tty_fd;
	
	tty_fd = open(tty_device, O_RDWR | O_NOCTTY | O_SYNC);
	if (tty_fd < 0) {
		LOG_PRINT_ERROR("Opening serial port: %m");
		return tty_fd;
	} else {
		tcflush(tty_fd, TCIFLUSH);
		//fcntl(tty_fd, F_SETFL, FASYNC);
		
		memset(&term, 0, sizeof(term));
		
		term.c_iflag = IGNBRK | IGNPAR /* bez parzystości */;
		term.c_oflag = term.c_lflag = term.c_line = 0;
		term.c_cflag = CREAD |  CLOCAL | tty_baud_flag | CS8 /* 8 bitow */;
		term.c_cc[VMIN]=1;
		term.c_cc[VTIME]=0;
		tcsetattr (tty_fd, TCSAFLUSH, &term);
		
		tcflush(tty_fd, TCIOFLUSH);
	}
	
	return tty_fd;
}

int init_net(const char* host, short port) {
	struct sockaddr_in netTerm;
	netTerm.sin_family=AF_INET;
	netTerm.sin_port=htons(port);
	netTerm.sin_addr.s_addr=inet_addr(host);

	int fd = socket(PF_INET, SOCK_STREAM, 0);
	connect(fd, (struct sockaddr*) &netTerm, sizeof(struct sockaddr_in));
	return fd;
}

uint8_t epso_checksum(uint8_t *buf, uint8_t len) {
	uint8_t i, sum = 0;
	for (i=0; i<len; i++) {
		sum ^= buf[i];
	}
	return sum | 0x20;
}

int epso_write(int tty_fd, uint8_t addr, uint8_t func, uint8_t data) {
	uint8_t buf[16], pos;
	int ret;
	
	if (func == 0xFF) {
		pos = snprintf((char*)buf, 15, "_S%02d%02X_X_", addr, func);
		buf[pos-2] = data;
	} else {
		pos = snprintf((char*)buf, 15, "_S%02d%02X_%d_", addr, func, data);
	}
	buf[0] = SOH;
	buf[6] = STX;
	buf[pos-1] = ETX;
	buf[pos] = epso_checksum(buf, pos);
	
	#ifdef DEBUG_OUT
	uint8_t i;
	for (i=0; i < pos+1; i++)
		printf("  >>> %02d: 0x%02x (%c)\n", i, buf[i], buf[i]);
	#endif
	
	ret = write(tty_fd, buf, pos+1);
	if (ret != pos+1) {
		LOG_PRINT_WARN("Error write data from device: %m");
		return -11;
	}
	return 0;
}

int epso_write_read(int tty_fd, uint8_t addr, uint8_t func, uint8_t data, char *buf_out, uint16_t buf_len) {
	uint8_t buft[260];
	uint8_t* buf = buft;
	uint8_t check_sum, i, j;
	int buf_pos, ret;
	struct timeval timeout;
	fd_set tty_fd_set;
	
	/// sending epso request
	ret = epso_write(tty_fd, addr, func, data);
	
	/// receive epso response
	FD_ZERO(&tty_fd_set);
	FD_SET(tty_fd, &tty_fd_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = TTY_READ_TIMEOUT_1;
	ret = select(tty_fd+1, &tty_fd_set, NULL, NULL, &timeout);
	if (ret > 0) {
		buf_pos = read(tty_fd, buf, 260);
		if (buf_pos < 0) {
			LOG_PRINT_WARN("Error read data from device - read: %m");
			return -21;
		}
		// doczytaie reszty pakietu
		do {
			FD_ZERO(&tty_fd_set);
			FD_SET(tty_fd, &tty_fd_set);
			timeout.tv_sec = 0;
			timeout.tv_usec = TTY_READ_TIMEOUT_2;
			ret = select(tty_fd+1, &tty_fd_set, NULL, NULL, &timeout);
			if (ret>0) {
				buf_pos += read(tty_fd, buf+buf_pos, 260-buf_pos);
				if (buf_pos < 0) {
					LOG_PRINT_WARN("Error read data from device - read (2): %m");
					return -22;
				}
			}
		} while (ret>0);
	} else if (ret == 0) {
		LOG_PRINT_WARN("Timeout on read from device: %d, func: 0x%02x", addr, func);
		return -20;
	} else {
		LOG_PRINT_WARN("Error read data from device - select: %m");
		return -23;
	}
	
	while (buf[0] == 0) {
		buf++;
		buf_pos--;
	}
	
	/// checking epso response
	if (buf_pos < 3) {
		LOG_PRINT_WARN("Too short response from device: %d, func: 0x%02x", addr, func);
		return -24;
	}
	check_sum = epso_checksum(buf, buf_pos-1);
	
	#ifdef DEBUG_IN
	for (i=0; i < buf_pos; i++)
		printf("  <<< %02d: 0x%02x (%c)\n", i, buf[i], buf[i]);
	#endif
	
	if ( buf[buf_pos-1] != check_sum) {
		LOG_PRINT_WARN("Error read data (func=0x%02x) from device %d - check_sum (0x%x != 0x%x)", func, addr, buf[buf_pos-1], check_sum);
		return -30;
	}
	
	j = -40;
	for (i=7; i < buf_pos-2; i++) {
		j = i - 7;
		if (j == buf_len) {
			LOG_PRINT_WARN("Output Buffer Overflow");
			return -32;
		} else {
			buf_out[j] = buf[i];
		}
	}
	
	return j+1;
}

/**
 * read data from roger card reader
 * 
 * return 0 when no read new data
 * return 1 when read card ID (put to @a card)
 * return 2 when read pin (put to @a pin)
 * return 3 when read card ID and pin
 */
char readCardPin(int serial, char *buf, int bufSize, uint64_t* card, uint64_t* pin, char **cardStr, char **pinStr) {
	char isNewData = 0;
	char *bufp;
	
	int len = epso_write_read(serial, 0x00, 0xA5, 62, buf, bufSize);
	buf[len]='\0';
	// printf("input status: %s\n", buf+len-2);
	
	if (len > 4) {
		if (buf[0] == 'R') {
			bufp = buf+1;
			bufp[16] = '\0';
			
			// now in bufp we have HEX coded card ID
			if (cardStr)
				*cardStr = bufp;
			*card = strtoull(bufp, NULL, 16);
			
			bufp = buf+18;
			isNewData |= 0x01;
		} else {
			bufp = buf+1;
		}
		
		if (bufp[0] != ':') {
			char *pe = index(bufp, ':');
			*pe = '\0';
			
			// now in bufp we have DEC coded pin
			if (pinStr)
				*pinStr = bufp;
			*pin = strtoul(bufp, NULL, 10);
			
			isNewData |= 0x02;
		}
	}
	
	return isNewData;
}

char readCardPin2(int serial, char* buf, int bufSize, uint64_t* cardNum, uint64_t* pinNum, char* cardStr, char* pinMd5Str) {
	char *pinStr, isNewData;
	int i;
	
	isNewData = readCardPin(serial, buf, bufSize, cardNum, pinNum, NULL, &pinStr);
	
	if ((isNewData & 0x01) && cardStr) { // card number in cardNum
		char cardLen = 0;
		if (*cardNum & 0x00ffffff00000000LL) {
			cardLen = 7;
		} else {
			cardLen = 4;
		}
		uint8_t *cardNumPtr = (uint8_t*)(cardNum);
		for (i=0; i<cardLen; ++i) {
			sprintf(cardStr + i*2, "%02X", (unsigned int)cardNumPtr[i]);
		}
		cardStr[cardLen*2] = 0;
	}
	
	if ((isNewData & 0x02) && pinMd5Str) { // pin in pinNum 
		unsigned char digest[16];
		MD5_CTX context;
		MD5_Init(&context);
		MD5_Update(&context, pinStr, strlen(pinStr));
		MD5_Final(digest, &context);
		
		for(i = 0; i < 16; ++i) {
			sprintf(pinMd5Str + i*2, "%02x", (unsigned int)digest[i]);
		}
		pinMd5Str[32] = 0;
	}
	
	return isNewData;
}
