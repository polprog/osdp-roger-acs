#include <stdint.h>
#include <stdbool.h>

enum InputTypes {
	DI_MANUAL_OPEN = 0x01,
	DI_IS_EMERGENCY_OPEN = 0x02,
	DI_IS_DOOR_CLOSE = 0x04,
	DI_IS_DOOR_LOCK = 0x08,
	DI_STATE_IS_DIFF = 0x80,
};

enum DoorOperations {
	DO_UNLOCK,
	DO_LOCK,
};

void set_door_state(enum DoorOperations doorOperation, int32_t mask);

void get_input_state(uint8_t *values, int32_t mask);
