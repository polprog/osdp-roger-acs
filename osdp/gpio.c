#include <gpiod.h>
#include <stdio.h>

#define DEVICE "gpiochip0"

int read_gpio_status(){
	char* device = DEVICE;
	int offsets[] = {14};
	int values[] = {0};

	gpiod_ctxless_get_value_multiple(device, offsets, values, 1, true, "gpioget");
	
	printf("%p: %x\n", values, values[0]);	
	return 0;
	
}

int write_gpio_status(){}

