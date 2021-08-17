#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "error_reporting.h"

#include "eventSend.h"

const char *eventHost = 0;
const char *logHost = 0;
const char *logPort = 0;

#define BUF_SIZE 256

struct TCP_Message {
	char data[ BUF_SIZE ];
	int  len;
	const char *dst_host;
	const char *dst_port;
};

int tcpSend(struct TCP_Message* msg) {
	int ret = -1;
	struct addrinfo *netInfo;
	
	ret = getaddrinfo(msg->dst_host, msg->dst_port, NULL, &netInfo);
	if (ret == 0) {
		struct addrinfo *netInfo2 = netInfo;
		while (netInfo2) {
			if (netInfo2->ai_socktype == SOCK_STREAM) {
				int sh = socket(netInfo2->ai_family, netInfo2->ai_socktype, 0);
				if (sh<0) {
					LOG_PRINT_ERROR("Error in socket(): %s", strerror(errno));
				} else {
					if (connect(sh, netInfo2->ai_addr, netInfo2->ai_addrlen)) {
						LOG_PRINT_ERROR("Error in connect(): %s", strerror(errno));
						close(sh);
					} else {
						ret = send(sh, msg->data, msg->len, 0);
						if (ret < 0) {
							LOG_PRINT_ERROR("Error in send(): %s", strerror(errno));
							close(sh);
						} else {
							close(sh);
							break;
						}
					}
				}
				ret = -2;
			}
			netInfo2=netInfo2->ai_next;
		}
	} else {
		LOG_PRINT_ERROR("getaddrinfo: %d %s", ret, strerror(errno));
		ret = -1;
	}
	
	if (netInfo) {
		freeaddrinfo(netInfo);
	}
	if (ret < 0) {
		LOG_PRINT_ERROR("error send \"%s\" to %s:%s", msg->data, msg->dst_host, msg->dst_port);
	}
	
	free(msg);
	return ret;
}

void remotePrintf(char* format, ...) {
	struct TCP_Message* msg = malloc( sizeof(struct TCP_Message) );
	va_list args;
	va_start (args, format);
	
	msg->len = vsnprintf (msg->data, BUF_SIZE, format, args);
	
	msg->dst_host = logHost;
	msg->dst_port = logPort;
	
	pthread_t sendThread;
	pthread_create( &sendThread, 0, (void* (*)(void *))tcpSend, msg );
	pthread_detach(sendThread);
}

#ifdef DEBUG
	#define LPRINT(...) printf(__VA_ARGS__)
#else
	#define LPRINT(...) if (logHost && logPort) { remotePrintf(__VA_ARGS__); }  syslog(LOG_INFO, __VA_ARGS__);
#endif

#define CHECK_BUFFER_OVERFLOW(size) if (size >= BUF_SIZE) { LOG_PRINT_ERROR("Event command buffer overflow"); return; }

void sendEvent(int eventType, const char* door, ...) {
	char cmdBuf[ BUF_SIZE ];

	int bufPos = 0;
	if (eventHost)
		bufPos = snprintf(cmdBuf, BUF_SIZE, "wget -q -O /dev/null 'http://%s/event?type=%s", eventHost, eventNames[eventType]);
	CHECK_BUFFER_OVERFLOW(bufPos);
	
	va_list vl;
	va_start(vl, door);
	
	switch (eventType) {
		case EVENT_AUTH_OK: {
			uint32_t    door_mask = va_arg(vl, uint32_t);
			const char* user = va_arg(vl, const char*);
			const char* card = va_arg(vl, const char*);
			LPRINT("ACCESS FOR %s (door_mask=0x%x) GRANTED FOR: %s (card=0x%s)\n", door, door_mask, user, card);
			if (strstr(door, "%"))
				bufPos += snprintf(cmdBuf + bufPos, BUF_SIZE - bufPos, "&door_mask=%d", door_mask);
			bufPos += snprintf(cmdBuf + bufPos, BUF_SIZE - bufPos, "&user=%s&card=%s", user, card);
			break;
		}
		case EVENT_AUTH_ERR_EXPIRE:
		case EVENT_AUTH_ERR_PIN:
		case EVENT_AUTH_ERR: {
			const char* user = va_arg(vl, const char*);
			const char* card = va_arg(vl, const char*);
			
			if (user) {
				LPRINT("ACCESS FOR %s DENIED FOR: %s (card=0x%s)%s\n", door, user, card, 
					(eventType == EVENT_AUTH_ERR_EXPIRE) ? " expire" : ((eventType == EVENT_AUTH_ERR_PIN) ? " invalid pin" : "")
				);
				bufPos += snprintf(cmdBuf + bufPos, BUF_SIZE - bufPos, "&user=%s", user);
			} else {
				LPRINT("ACCESS FOR %s DENIED FOR: card=0x%s\n", door, card);
			}
			bufPos += snprintf(cmdBuf + bufPos, BUF_SIZE - bufPos, "&card=%s", card);
			break;
		}
		case EVENT_MANUAL_OPEN: {
			LPRINT("MANUAL OPEN %s\n", door);
			break;
		}
		case EVENT_DOOR_UNLOCK: {
			uint32_t    door_mask = va_arg(vl, uint32_t);
			LPRINT("DOOR UNLOCK %s (door_mask=0x%x)\n", door, door_mask);
			if (strstr(door, "%"))
				bufPos += snprintf(cmdBuf + bufPos, BUF_SIZE - bufPos, "&door_mask=%d", door_mask);
			break;
		}
		case EVENT_DOOR_LOCK: {
			uint32_t    door_mask = va_arg(vl, uint32_t);
			LPRINT("DOOR LOCK %s (door_mask=0x%x)\n", door, door_mask);
			if (strstr(door, "%"))
				bufPos += snprintf(cmdBuf + bufPos, BUF_SIZE - bufPos, "&door_mask=%d", door_mask);
			break;
		}
		case EVENT_DOOR_IS_OPEN: {
			LPRINT("DOOR OPEN %s\n", door);
			break;
		}
		case EVENT_DOOR_IS_CLOSE: {
			LPRINT("DOOR CLOSE %s\n", door);
			break;
		}
		case EVENT_DOOR_IS_UNLOCK: {
			LPRINT("DOOR OPEN %s\n", door);
			break;
		}
		case EVENT_DOOR_IS_LOCK: {
			LPRINT("DOOR CLOSE %s\n", door);
			break;
		}
		case EVENT_DOOR_EMERGENCY_ACTIVE: {
			LPRINT("DOOR EMERGENCY OPEN ACTIVE %s\n", door);
			break;
		}
		case EVENT_DOOR_EMERGENCY_INACTIVE: {
			LPRINT("DOOR EMERGENCY OPEN INACTIVE %s\n", door);
			break;
		}
		case EVENT_DOOR_OPEN_ALARM: {
			uint32_t    mask = va_arg(vl, uint32_t);
			LPRINT("DOOR OPEN ALARM FOR %s (mask=0x%x)\n", door, mask);
			break;
		}
	}
	CHECK_BUFFER_OVERFLOW(bufPos);
	
	va_end(vl);
	
	bufPos += snprintf(cmdBuf + bufPos, BUF_SIZE - bufPos, "' &");
	CHECK_BUFFER_OVERFLOW(bufPos);
	
	if (eventHost) {
		#ifdef DEBUG
		printf("run event cmd: %s\n", cmdBuf);
		#endif
		system(cmdBuf);
	}
}
