#include <sqlite3.h>
#include "door_controller.h"

struct Controler {
	int sh;
	Door* doors;
	int doorsCount;
	sqlite3 *database;
};

void check_network(struct Controler *ctrl);

int init_remote_control(int port, const char* statusDbPath, Door* doors, int doorsCount, struct Controler *ctrl);

