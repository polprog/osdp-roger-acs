#ifndef EVENT_SEND_H
#define EVENT_SEND_H

extern const char *eventHost;
extern const char *logHost;
extern const char *logPort;

enum {
	EVENT_AUTH_OK,
	EVENT_AUTH_ERR,
	EVENT_AUTH_ERR_EXPIRE,
	EVENT_AUTH_ERR_PIN,
	
	EVENT_MANUAL_OPEN,
	EVENT_DOOR_UNLOCK,
	EVENT_DOOR_LOCK,
	
	EVENT_DOOR_IS_OPEN,
	EVENT_DOOR_IS_CLOSE,
	EVENT_DOOR_IS_UNLOCK,
	EVENT_DOOR_IS_LOCK,
	
	EVENT_DOOR_EMERGENCY_ACTIVE,
	EVENT_DOOR_EMERGENCY_INACTIVE,
	EVENT_DOOR_OPEN_ALARM,

	EVENT_REMOTE_CTRL,
};

static const char* const eventNames[] = {
    "auth_ok", "auth_err", "auth_err_expire", "auth_err_pin",
	"manual_open", "door_unlock", "door_lock",
	"door_is_open", "door_is_close", "door_is_unlock", "door_is_lock",
	"door_emergency_active", "door_emergency_inactive", "door_open_alarm",
};

void sendEvent(int eventType, const char* door, ...);

#endif
