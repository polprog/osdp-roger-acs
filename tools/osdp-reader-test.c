#include "readers/osdp/osdp.h"

#define OSDP_SLEEP_TIME 50000

int main() {
	int readerAddress = 0x7f /* 0x7f is broadcast address */;
	int readerSerial = portsetup("/dev/ttyS2", 1 /*use_rs485*/);
	if ( readerSerial < 0 ) {
		printf("Open TTY ERROR\n");
		return 1;
	}
	
	struct osdp_packet packet, answer;
	struct osdp_response res;
	
	while(1) {
		fill_packet(&packet, readerAddress, osdp_POLL, NULL, 0);
		send_packet(&packet, readerSerial);
		#ifdef DEBUG
		packet_dump(&packet, 0, 0);
		#endif
		
		usleep(OSDP_SLEEP_TIME);
		
		if(recv_packet(&answer, readerSerial)) {
			process_packet(&answer, &res);
			#ifdef DEBUG
			printf("RES code= %02xh len= %d\n", res.response, res.payloadlen);
			#endif
			switch(res.response){
				case osdp_RAW:
					printf("enter card: 0x");
					for (int i=0; i<res.payloadlen; ++i)
						printf("%02x", res.payload[i]);
					printf("\n");
					break;
				
				case osdp_KPD:
					// received pin data ...
					for(int i = 0; i < res.payloadlen; i++){
						if(res.payload[i] == 0x7f){ // * on pinpad - clear pin
							printf("clear pin\n");
						} else if(res.payload[i] == 0x0d){ // # on pinpad - pin done
							printf("pin accepted\n");
						} else {
							printf("add %c to pin\n", res.payload[i]);
						}
					}
				
				case osdp_ACK:
					printf("ACK - all OK, but no new data from reader\n");
					break;
				case osdp_NACK:
					printf("Received NACK from reader\n");
					break;
			}
		} else {
			fprintf(stderr, "Received bad packet\n");
			tcflush(readerSerial, TCIOFLUSH);
		}
	}
}
