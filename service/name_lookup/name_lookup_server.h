#pragma once
#include <stdint.h>

#define NOZA_MAX_SERVICE    16 // TODO: leave it to options in makefile

#define NAME_SERVER_VID     0

enum {
	NAME_LOOKUP_REGISTER = 1,
	NAME_LOOKUP_RESOLVE_NAME,
	NAME_LOOKUP_RESOLVE_ID,
	NAME_LOOKUP_UNREGISTER,
};

enum {
	NAME_LOOKUP_OK = 0,
	NAME_LOOKUP_ERR_DUPLICATE = -1,
	NAME_LOOKUP_ERR_NOT_FOUND = -2,
	NAME_LOOKUP_ERR_CAPACITY = -3,
	NAME_LOOKUP_ERR_INVALID = -4,
};

typedef struct name_msg_s {
	uint32_t cmd;
	const char *name;
	uint32_t service_id;
	uint32_t vid;
	int32_t code;
} name_msg_t;
