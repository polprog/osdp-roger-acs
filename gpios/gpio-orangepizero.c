#include <stdio.h>
#include <gpiod.h>
#include <string.h>

#include "error_reporting.h"
#include "gpios/gpio.h"

#ifndef GPIO_DEVICE
#define GPIO_DEVICE "gpiochip0"
#endif

enum {
	DO_UNLOCK_DOOR_1 = 13, // PA13
	DI_MANUAL_OPEN_1 = 14, // PA14
	DI_IS_EMERGENCY_OPEN_1 = 15, // PA15
	DI_IS_DOOR_LOCK_1 = 16, // PA16
	
	DO_UNLOCK_DOOR_2 = 7, // PA7
	DI_MANUAL_OPEN_2 = 6, // PA6
	DI_IS_EMERGENCY_OPEN_2 = 11, // PA11
	DI_IS_DOOR_LOCK_2 = 12, // PA12
	
	PA18 = 18,
	PA19 = 19,
	
	PG06 = 6*32+6,
	PG07 = 6*32+7,
};

#define DOOR_OUTPUTS_NUM 4
const unsigned int outputs[] = {DO_UNLOCK_DOOR_1, DO_UNLOCK_DOOR_2, PG06, PG07};

#define INPUTS_DOORS 2
#define INPUTS_TYPES 4
const unsigned int inputs[] = {
	DI_MANUAL_OPEN_1, DI_IS_EMERGENCY_OPEN_1, DI_IS_DOOR_LOCK_1, PA18,
	DI_MANUAL_OPEN_2, DI_IS_EMERGENCY_OPEN_2, DI_IS_DOOR_LOCK_2, PA19,
};

void set_door_state(enum DoorOperations doorOperation, int32_t mask) {
	int val = (doorOperation == DO_UNLOCK) ? 0 : 1;
	
	for (int i=0; i<DOOR_OUTPUTS_NUM; ++i) {
		if (mask & (1<<i)) {
			int ret  __attribute__((unused));
			ret = gpiod_ctxless_set_value(GPIO_DEVICE, outputs[i], val, false, "gpioset", 0, 0);
			#if GPIO_DEBUG
			printf("Do door operation %d, mask=%04X, ret=%d\n", doorOperation, mask, ret);
			#endif
		}
	}
}

void get_input_state(uint8_t *values, int32_t mask) {
	int inputsValues[INPUTS_DOORS * INPUTS_TYPES];
	memset(inputsValues, 0x13, sizeof(int)*INPUTS_DOORS * INPUTS_TYPES);
	int ret  __attribute__((unused));
	ret = gpiod_ctxless_get_value_multiple(GPIO_DEVICE, inputs, inputsValues, INPUTS_DOORS * INPUTS_TYPES, false, "gpioget");
	
	#if GPIO_DEBUG
	printf("input read (status=%d) values: ", ret);
	for (int i=0; i<INPUTS_DOORS * INPUTS_TYPES; ++i) {
		printf("%d->%d ", inputs[i], inputsValues[i]);
	}
	printf("\n");
	#endif
	
	for (int i=0; mask && i<INPUTS_DOORS; ++i) {
		if (mask & 0x01) {
			values[i] = 0;
			for (int j=0; j< INPUTS_TYPES; ++j)
				values[i] |= inputsValues[i*4 + j] << j;
		}
		mask = mask >> 1;
	}
}
