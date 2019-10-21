/*
 * System kontroli dostępu w oparciu o kartę zbliżeniową i PIN
 * z wykorzystaniem kontrolera NPE, czytnika z komunikacją szeregową
 * (RS232) i protokołem EPSO firmy Roger (np. PRT12EM)
 */

#define EVENT_HOST "192.168.209.13"

//#define CARD_AND_PIN
//#define CARD_OR_PIN
#define ONLY_CARD

//#define WINDA
//#define KLUCZE

//#define DEBUG


#define NO_USE_SYSLOG
#include <syslog.h>
#include <errno.h>
#include "roger.c"

#include <sqlite3.h>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DATABESE_FILE "kd-ocean_v4.db"

#define BUF_SIZE 500
char dataBuf[ BUF_SIZE ];

enum States {
	WAIT_FOR_FIRST_ID,
	HAS_PIN,
	HAS_CARD,
	HAS_FIRST_ID,
	HAS_SECOND_ID,
	HAS_USERNAME,
};

int tcpSend(int len, char* data, char* host, short port) {
	int sh = socket(PF_INET, SOCK_STREAM, 0);
	if (sh<0) {
		LOG_PRINT_ERROR("Error in socket(): %s", strerror(errno));
		return -1;
	}
	
	struct sockaddr_in dstAddr;
	dstAddr.sin_family=PF_INET;
	dstAddr.sin_port=htons(port);
	inet_aton(host, &dstAddr.sin_addr);
	
	if (connect(sh, (struct sockaddr *)&dstAddr, sizeof(dstAddr))) {
		LOG_PRINT_ERROR("Error in connect(): %s", strerror(errno));
		close(sh);
		return -2;
	}
	
	if (send(sh, data, len, 0) < 0) {
		LOG_PRINT_ERROR("Error in send(): %s", strerror(errno));
		close(sh);
		return -3;
	}

	close(sh);
	return len;
}
void sendEvent(const char* door, const char* type, const char* card, const char* user) {
	char buf[254];
	snprintf(buf, 254, "wget 'http://" EVENT_HOST "/event?door=%s&event_type=%s&card=%s&user=%s' &",
		door,
		type,
		card ? card : "",
		user ? user : ""
	);
	puts(buf);
	system(buf);
}

#ifdef DEBUG
	#define DPRINT(...) printf(__VA_ARGS__)
	#define LPRINT(...) printf(__VA_ARGS__)
#else
	//#define DPRINT(...) ;
	#define DPRINT(...) printf(__VA_ARGS__)
	#include <stdarg.h>
	void LPRINT(char* format, ...) {
		va_list args;
		va_start (args, format);
		int len = vsnprintf (dataBuf, BUF_SIZE, format, args);
		tcpSend(len, dataBuf, EVENT_HOST, 5678);
	}
#endif



#ifdef ROGER
#include "roger_logic.c"
#endif
#ifdef IDESCO
#include "idesco_logic.c"
#endif

