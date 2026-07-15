#include "defs.h"

// Cmd definition
enum HOST_COMMAND_TYPE {
	HOST_READ = 0,
	HOST_WRITE,
	HOST_NONE
};

// Host-side Command
typedef struct host_command {
	HOST_COMMAND_TYPE type;
	lpa_t LPA;	
	unit_t data;
} host_command_t;
