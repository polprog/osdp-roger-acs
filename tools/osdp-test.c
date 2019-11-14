#include <stdio.h>
#include <termios.h>
#include "util.h"
#include "osdp.h"

int main(int argc, char** argv){
	

	char* portTX = TTY;
	struct termios optionsTX;
	int portTXfd;

	struct osdp_packet packet, answer;
	struct osdp_response res;
	portTXfd = open_port(portTX);
	
	portsetup(portTXfd, &optionsTX);

	printf("Init done\nFD: TX: %d\n", portTXfd);
	char i = 0;
/*	
	for(i = 0; i < 255; i++){
		char beep[] = {0, i, 1, 1, 3};
		printf("Tonecode=%d\n", i);
		fill_packet(&packet, 0x7f, osdp_BUZ, beep, 5);
		send_packet(&packet, portTXfd);
		sleep(2);
	}
*/	
	
	
	
	while(true){
		fill_packet(&packet, 0x55, osdp_POLL, NULL, 0);
		send_packet(&packet, portTXfd);
		sleep(1);
		if(recv_packet(&answer, portTXfd)){
			process_packet(&answer, &res); packet_dump(&answer);
			printf("RES len = %d\n", res.payloadlen);
		}else{
			fprintf(stderr, "Received bad packet\n");
			tcflush(portTXfd, TCIOFLUSH);
			close(portTXfd);
			portTXfd = open_port(portTX);
			portsetup(portTXfd, &optionsTX);
			printf("Port reopepend and initialized\n");
		}
		ledset(&packet, 0x7f, 0, AMBER, true, 6);
		send_packet(&packet, portTXfd);
		sleep(1);
		tcdrain(portTXfd);
		ledset(&packet, 0x7f, 1, RED, false, 0);
		send_packet(&packet, portTXfd);
		sleep(1);
		tcdrain(portTXfd);
		ledset(&packet, 0x7f, 0, RED, false, 0);
		send_packet(&packet, portTXfd);
		sleep(1);
		tcdrain(portTXfd);
		beepset(&packet, 0x7f, 3, 3, 4);
		send_packet(&packet, portTXfd);
		sleep(1);
		
		if(recv_packet(&answer, portTXfd)){
			process_packet(&answer, &res); packet_dump(&answer);
		}	
		tcflush(portTXfd, TCIOFLUSH);
		printf("----------\n");
	}
	exit(0);

	return 0;
}
