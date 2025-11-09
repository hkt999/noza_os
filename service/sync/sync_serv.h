#pragma once
#include <stdint.h>
#include "posix/errno.h"

#define NOZA_SYNC_SERVICE_NAME "noza_sync"

enum {
	MUTEX_SUCCESS = 0,
	MUTEX_LOCK_FAIL = EBUSY,
	MUTEX_NOT_ENOUGH_RESOURCE = ENOMEM,
	MUTEX_INVALID_ID = ESRCH,
	MUTEX_INVALID_TOKEN = EINVAL,
	COND_SUCCESS = 0,
	COND_NOT_ENOUGH_RESOURCE = ENOMEM,
	COND_INVALID_ID = ESRCH,
	COND_INVALID_TOKEN = EINVAL,
	SEM_SUCCESS = 0,
	SEM_TRYWAIT_FAIL = EAGAIN,
	SEM_NOT_ENOUGH_RESOURCE = ENOMEM,
	SEM_INVALID_ID = ESRCH,
	SEM_INVALID_TOKEN = EINVAL,
	INVALID_OP = EINVAL
};

// service call command
#define MUTEX_ACQUIRE	1
#define MUTEX_RELEASE	2
#define MUTEX_LOCK		3
#define MUTEX_TRYLOCK	4
#define MUTEX_UNLOCK	5
#define MUTEX_TAIL		6

#define COND_ACQUIRE	7
#define COND_RELEASE	8
#define COND_WAIT		9
#define COND_TIMEDWAIT	10
#define COND_SIGNAL		11
#define COND_BROADCAST	12
#define COND_TAIL		13

#define SEM_ACQUIRE		14
#define SEM_RELEASE		15
#define SEM_WAIT		16
#define SEM_TRYWAIT		17
#define SEM_POST		18
#define SEM_GETVALUE	19
#define SEM_TAIL		20

// resource
#define MAX_LOCKS		16
#define MAX_CONDS		16
#define MAX_SEMS		16
#define MAX_PENDING		16

typedef struct {
	uint32_t cmd;		// mutex command
	uint32_t mid;		// mutex id
	uint32_t token;		// mutex access token
	uint32_t code;		// mutex return code
} mutex_msg_t;

typedef struct {
	uint32_t cmd;		// cond command
	uint32_t mid;		// cond user mutex id
	uint32_t mtoken;
	uint32_t code;		// cond return code
	uint32_t cid;		// cond id
	uint32_t ctoken;	// cond access token
} cond_msg_t;

typedef struct {
	uint32_t cmd;		// semaphore command
	uint32_t sid;		// semaphore id
	uint32_t stoken;	// semaphore access token
	uint32_t value;		// semaphore value
	uint32_t code;		// semaphore return code
} sem_msg_t;

typedef mutex_msg_t packet_t;
