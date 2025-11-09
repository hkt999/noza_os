#pragma once

#include "setjmp.h"
#include "spinlock.h"
#include "type/hashslot.h"

#define SERVICE_PRIORITY	0
#define NOZA_VID_AUTO    0xFFFFFFFFu

typedef struct thread_record_s {
	uint32_t reserved_vid; // must stay first: kernel reads this directly
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
