#pragma once
#include <stdint.h>

enum {
	MEMORY_SUCCESS = 0,
    MEMORY_INVALID_OP,
};

// syscall command
#define MEMORY_MALLOC	1
#define MEMORY_FREE	    2

typedef struct {
	uint32_t    cmd;
    uint32_t    size;
    void        *ptr;
    uint32_t    code;
} mem_msg_t;