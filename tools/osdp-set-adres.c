#include <stdio.h>
#include <termios.h>
#include "util.h"
#include "osdp.h"

int main(int argc, char** argv){
	
	if(argc < 2){
		fprintf(stderr, "Usage: %s <address hex>\n", argv[0]);
		exit(1);
	}
	int address = strtol(argv[1], NULL, 16);

	int osdpSerial = portsetup(TTY_PORT, 1);
	printf("Init done\nFD: TX: %d\n", osdpSerial);

	struct osdp_packet packet, answer;
	struct osdp_response res;
	comset(&packet, address, 9600);
	send_packet(&packet, osdpSerial);
	printf("Address set to %02x\n", address);
	sleep(1);
	if(recv_packet(&answer, osdpSerial)){
		process_packet(&answer, &res); packet_dump(&answer);
	}else{
		fprintf(stderr, "Received bad packet\n");
		tcflush(osdpSerial, TCIOFLUSH);
		close(osdpSerial);
		osdpSerial = portsetup(TTY_PORT, 1);
		printf("Port reopepend and initialized\n");
	}
	printf("Response received OK\n");

	return 0;
}
