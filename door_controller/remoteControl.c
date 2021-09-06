#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "remoteControl.h"
#include "error_reporting.h"
#include "eventSend.h"

int open_network(short port) {
	int sh = socket(AF_INET6, SOCK_DGRAM, 0);
	if (sh<0) {
		LOG_PRINT_ERROR("error in socket: %d %s\n", sh, strerror(errno));
		return -1;
	}
	
	int res = 0;
	res = setsockopt(sh, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&res, sizeof(res)); 
	if (res<0) {
		LOG_PRINT_ERROR("error in setsockopt: %d %s\n", res, strerror(errno));
		return -2;
	}
	
	struct sockaddr_in6 addr;
	addr.sin6_family=AF_INET6;
	addr.sin6_port=htons( port );
	addr.sin6_addr=in6addr_loopback;
	
	res=bind(sh, (struct sockaddr *) &addr, sizeof(addr));
	if (res<0) {
		LOG_PRINT_ERROR("error in bind: %d %s\n", res, strerror(errno));
		return -3;
	}
	
	return sh;
}

enum DbSetUnset {
	DB_SET,
	DB_UNSET
};

int db_set_unset(const char* door, const char* state, int arg, enum DbSetUnset action, struct Controler *ctrl) {
	sqlite3_stmt *stmt;
	if (action == DB_SET) {
		sqlite3_prepare_v2( ctrl->database, "INSERT OR REPLACE INTO 'state' VALUES (?, ?, ?);", -1, &stmt, NULL );
	} else if (action == DB_UNSET) {
		sqlite3_prepare_v2( ctrl->database, "DELETE FROM 'state' WHERE door=? AND mode=?;", -1, &stmt, NULL );
	} else {
		return 1;
	}
	sqlite3_bind_text(stmt, 1, door, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, state, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, arg);
	int ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		LOG_PRINT_ERROR("sql set/uset error: %d\n", ret);
		return 2;
	}
	sqlite3_finalize(stmt);
	return 0;
}

#define BUF_SIZE 128

void check_network(struct Controler *ctrl) {
	fd_set waitFdSet;
	FD_ZERO(&waitFdSet);
	FD_SET(ctrl->sh, &waitFdSet);
	
	struct timeval timeout;
	timeout.tv_sec  = 0;
	timeout.tv_usec = 0;
	
	int ret = select(ctrl->sh + 1, &waitFdSet, 0, 0, &timeout);
	
	if (ret == -1) {
		LOG_PRINT_WARN("select: %m\n");
	} else if (ret != 0) {
		struct sockaddr_in6 from;
		socklen_t fromlen = sizeof(from);
		
		char buf[BUF_SIZE];
		int ret = recvfrom(ctrl->sh, buf, BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *) &from, &fromlen);
		if (ret > 0) {
			buf[ret] = 0;
			char *tmp = NULL;
			char *cmd  = strtok_r(buf,  " \t\n", &tmp);
			char *door = strtok_r(NULL, " \t\n", &tmp);
			char *arg1 = strtok_r(NULL, " \t\n", &tmp);
			char *argX = strtok_r(NULL, " \t\n", &tmp);
			int arg2 = 0;
			if (argX) arg2 = strtoll(argX, 0, 0);
			
			sendEvent(EVENT_REMOTE_CTRL, door, cmd, arg1, argX);

			if (!cmd || !door || !arg1) {
				sendto(ctrl->sh, "ERR\n", 4, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
				return;
			}

			Door* doorObj = getDoorByName(ctrl->doors, ctrl->doorsCount, door);
			if (!doorObj) {
				sendto(ctrl->sh, "ERR\n", 4, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
				return;
			}
			
			if (strcmp(cmd, "get") == 0) {
				if (strcmp(arg1, "info") == 0) {
					char buf2[BUF_SIZE];
					int size = snprintf(buf2, BUF_SIZE, "INFO %s: 0x%x %d %d\n", door, doorObj->lastInput, doorObj->alarm_timers, doorObj->door_lock_timer);
					if (size < BUF_SIZE) {
						sendto(ctrl->sh, buf2, size, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen); 
						return;
					}
				} else if (strcmp(arg1, "dbinfo") == 0) {
					sqlite3_stmt *stmt;
					sqlite3_prepare_v2( ctrl->database, "SELECT GROUP_CONCAT(mode) FROM state WHERE door=?", -1, &stmt, NULL );
					sqlite3_bind_text(stmt, 1, door, -1, SQLITE_STATIC);
					const char *info = NULL;
					if (sqlite3_step(stmt) == SQLITE_ROW) {
						info = (const char*) sqlite3_column_text(stmt, 0);
					}
					char buf2[BUF_SIZE];
					int size = snprintf(buf2, BUF_SIZE, "DB INFO %s: %s\n", door, info);
					sqlite3_finalize(stmt);
					if (size < BUF_SIZE) {
						sendto(ctrl->sh, buf2, size, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
						return;
					}
				}
			} else if (strcmp(cmd, "set") == 0 && argX) {
				int ret = 0;
				if (strcmp(arg1, "unlock") == 0) {
					doorObj->admDisableMask = 0;
					doorObj->airLockMask = 0;
					unlock_door(doorObj, arg2);
					doorObj->admDisableMask = arg2;
					ret += db_set_unset(door, "lock",   arg2, DB_UNSET, ctrl);
					ret += db_set_unset(door, "unlock", arg2, DB_SET, ctrl);
					signalDoorStatus(doorObj);
				} else if (strcmp(arg1, "lock") == 0) {
					doorObj->admDisableMask = 0;
					doorObj->airLockMask = 0;
					lock_door(doorObj, arg2);
					doorObj->admDisableMask = arg2;
					ret += db_set_unset(door, "unlock", arg2, DB_UNSET, ctrl);
					ret += db_set_unset(door, "lock",   arg2, DB_SET, ctrl);
					signalDoorStatus(doorObj);
				} else {
					ret = 1;
				}
				if (ret == 0) {
					sendto(ctrl->sh, "OK\n", 3, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
					return;
				}
			} else if (strcmp(cmd, "unset") == 0) {
				if (db_set_unset(door, arg1, arg2, DB_UNSET, ctrl) == 0) {
					if (strcmp(arg1, "unlock") == 0 || strcmp(arg1, "lock") == 0) {
						doorObj->admDisableMask = 0;
						signalDoorStatus(doorObj);
					}
					sendto(ctrl->sh, "OK\n", 3, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
					return;
				}
			} else if (strcmp(cmd, "do") == 0 && argX) {
				if (strcmp(arg1, "unlock") == 0) {
					if (unlock_door(doorObj, arg2)) {
						sendto(ctrl->sh, "OK\n", 3, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
						signalDoorStatus(doorObj);
						return;
					}
				} else if (strcmp(arg1, "lock") == 0) {
					if (lock_door(doorObj, arg2)) {
						sendto(ctrl->sh, "OK\n", 3, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
						signalDoorStatus(doorObj);
						return;
					}
				} else if (strcmp(arg1, "block") == 0) {
					doorObj->airLockMask |= arg2;
					sendto(ctrl->sh, "OK\n", 3, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
					return;
				} else if (strcmp(arg1, "unblock") == 0) {
					doorObj->airLockMask &= ~arg2;
					sendto(ctrl->sh, "OK\n", 3, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
					return;
				}
			}
			sendto(ctrl->sh, "ERR\n", 4, MSG_DONTWAIT, (struct sockaddr *) &from, fromlen);
		}
	}
}


int init_remote_control(int port, const char* statusDbPath, Door* doors, int doorsCount, struct Controler *ctrl) {
	ctrl->sh = open_network(port);
	ctrl->doors = doors;
	ctrl->doorsCount = doorsCount;
	if (ctrl->sh < 0)
		return -1;

	// open database
	if ( sqlite3_open(statusDbPath, &(ctrl->database)) ) {
		LOG_PRINT_CRIT("Can't open database: %s\n", sqlite3_errmsg(ctrl->database));
		sqlite3_close(ctrl->database);
		return -2;
	}

	sqlite3_stmt *stmt;

	int ret = sqlite3_prepare_v2( ctrl->database, "CREATE TABLE IF NOT EXISTS 'state' (door TEXT, mode TEXT, arg INT, UNIQUE(door, mode));", -1, &stmt, NULL );
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		LOG_PRINT_ERROR("sqlite3_step %d\n", ret);
		return -3;
	}
	sqlite3_finalize(stmt);
	
	ret = sqlite3_prepare_v2( ctrl->database, "SELECT * from 'state';", -1, &stmt, NULL );
	while ( ( ret = sqlite3_step(stmt) ) == SQLITE_ROW ) {
		const char* door = (const char*) sqlite3_column_text(stmt, 0);
		const char* arg1 = (const char*) sqlite3_column_text(stmt, 1);
		int arg2 = sqlite3_column_int(stmt, 2);

		DPRINT("%s -- %s -- %d\n", door, arg1, arg2);

		Door* doorObj = getDoorByName(ctrl->doors, ctrl->doorsCount, door);
		if (!doorObj) {
			continue;
		}

		if (strcmp(arg1, "unlock") == 0) {
			unlock_door(doorObj, arg2);
			doorObj->admDisableMask = arg2;
		} else if (strcmp(arg1, "lock") == 0) {
			lock_door(doorObj, arg2);
			doorObj->admDisableMask = arg2;
		}
		signalDoorStatus(doorObj);
	}
	sqlite3_finalize(stmt);

	return 0;
}
