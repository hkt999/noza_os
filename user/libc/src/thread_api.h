#pragma once

#include "setjmp.h"
#include "spinlock.h"
#include "type/hashslot.h"

#define NOZA_ROOT_STACK_SIZE 2048
#define SERVICE_PRIORITY	0

typedef struct thread_record_s {
	int (*user_entry)(void *param, uint32_t tid);
	void *user_param;
	uint32_t *stack_ptr;
	uint32_t stack_size;
	uint32_t priority;
	uint32_t need_free_stack;
	void *thread_data;
	jmp_buf jmp_buf;
	void *process;
	uint32_t errno;
	hash_item_t hash_item;
} thread_record_t;

thread_record_t *get_thread_record(uint32_t pid);