#include <stdio.h>
#include "readers/osdp/osdp.h"

int main(int argc, char** argv){
	if(argc < 3){
		fprintf(stderr, "Usage: %s <serial> <address>\n", argv[0]);
		exit(1);
	}
	
	int osdpSerial = portsetup(argv[1], 1);
	int newAddress = strtol(argv[2], NULL, 0);
	
	printf("Init done\nFD: TX: %d\n", osdpSerial);
	struct osdp_packet packet, answer;
	struct osdp_response res;
	comset(&packet, 0x07f, newAddress, 9600);
	send_packet(&packet, osdpSerial);
	
	printf("Address set to %02x\n", newAddress);
	sleep(1);
	
	if(recv_packet(&answer, osdpSerial)){
		process_packet(&answer, &res);
		packet_dump(&answer, 0, 0);
		printf("Response received OK\n");
	}else{
		printf("Received bad packet\n");
	}

	return 0;
}
