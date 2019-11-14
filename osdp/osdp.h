#ifndef _OSDP_H
#define _OSDP_H
#include <termios.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <errno.h>
#include <linux/serial.h>
#include "util.h"
#include "crctable.h"

#define PACKETLEN 64

/* OSDP command/reply codes */
#define osdp_POLL 0x60
#define osdp_LED 0x69
#define osdp_BUZ 0x6a
#define osdp_COMSET 0x6e

#define osdp_ACK 0x40
#define osdp_NACK 0x41
#define osdp_RAW 0x50
#define osdp_KPD 0x53
#define osdp_COM 0x54

/* OSDP led colors */
#define BLACK 0
#define RED 1
#define GREEN 2
#define AMBER 3
#define BLUE 4

struct osdp_packet {
	unsigned int len;
	char data[PACKETLEN];
};

struct osdp_response {
	char response;
	char payloadlen;
	char payload[PACKETLEN];
};

void crclen_packet(struct osdp_packet *packet);
void fill_packet(struct osdp_packet *packet, char address, char command, void* data, int datalen);
bool portsetup(int portfd, struct termios *options, bool useRS485);
uint16_t fCrcBlk( uint8_t *pData, uint16_t nLength);
void packet_dump(struct osdp_packet *packet);
bool send_packet(struct osdp_packet *packet, int fd);
bool recv_packet(struct osdp_packet *packet, int fd);
bool process_packet(struct osdp_packet *packet, struct osdp_response *response);
void comset(struct osdp_packet *packet, char address, uint32_t baudrate);
void ledset(struct osdp_packet *packet, char address, char lednum, char color, bool blink, int blink_time);
void beepset(struct osdp_packet *packet, char address, char on_time, char off_time, char count);
#endif //_OSDP_H
