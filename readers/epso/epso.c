/*
 * comunication with EPSO reader by Roger (eg. PRT12EM)
 * 
 * Copyright (c) 2013-2019 Robert Paciorek,
 * 3-clause BSD license
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "readers/epso/epso.h"
#include "error_reporting.h"

#define TTY_READ_TIMEOUT_1 2000000
#define TTY_READ_TIMEOUT_2   40000

#define SOH 0x01
#define STX 0x02
#define ETX 0x03

#define BUF_SIZE 128

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
		pos = snprintf((char*)buf, 15, "_S%02d%02X_%c_", addr, func, data);
	} else {
		pos = snprintf((char*)buf, 15, "_S%02d%02X_%d_", addr, func, data);
	}
	if (pos > 15) pos = 15; // this should never happen
	buf[0] = SOH;
	buf[6] = STX;
	buf[pos-1] = ETX;
	buf[pos] = epso_checksum(buf, pos);
	
	#ifdef EPSO_DEBUG
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

int epso_write_read(int tty_fd, uint8_t addr, uint8_t func, uint8_t data, uint8_t *buf_out, uint16_t buf_len) {
	uint8_t *buf = buf_out;
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
		buf_pos = read(tty_fd, buf, buf_len);
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
				buf_pos += read(tty_fd, buf+buf_pos, buf_len-buf_pos);
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
	
	#ifdef EPSO_DEBUG
	for (i=0; i < buf_pos; i++)
		printf("  <<< %02d: 0x%02x (%c)\n", i, buf[i], buf[i]);
	#endif
	
	if ( buf[buf_pos-1] != check_sum) {
		LOG_PRINT_WARN("Error read data (func=0x%02x) from device %d - check_sum (0x%x != 0x%x)", func, addr, buf[buf_pos-1], check_sum);
		return -30;
	}
	
	if (!buf_out)
		return 0;
	
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
char readCardPin(int serial, uint8_t addr, char *buf, int bufSize, uint64_t* card, uint64_t* pin, char **cardStr, char **pinStr) {
	char isNewData = 0;
	char *bufp;
	
	int len = epso_write_read(serial, addr, 0xA5, 0x33, (uint8_t*)buf, bufSize); // data (range 1-255, send 0x33) is ignored in 0xA5 function
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
