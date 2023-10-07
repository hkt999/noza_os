#pragma once
#include <stdint.h>

enum {
	MUTEX_SUCCESS = 0,
	MUTEX_LOCK_FAIL,
	MUTEX_NOT_ENOUGH_RESOURCE,
	MUTEX_INVALID_ID,
	MUTEX_INVALID_TOKEN,
	MUTEX_INVALID_OP,
};

// syscall command
#define MUTEX_ACQUIRE	1
#define MUTEX_RELEASE	2
#define MUTEX_LOCK		3
#define MUTEX_TRYLOCK	4
#define MUTEX_UNLOCK	5

#define MAX_LOCKS		16
#define MAX_PENDING		16

typedef struct {
	uint32_t cmd;
	uint32_t mid;
	uint32_t token;
	uint32_t code;
} mutex_msg_t;