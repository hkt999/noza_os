#pragma once

#include "type/dblist.h"
#include "tinyalloc_port/tinyalloc.h"
#include "spinlock.h"

#define NUM_PROCESS 64

typedef struct env_s {
	int argc;
	char **argv;
} env_t;

typedef int (*main_t)(int argc, char **argv);

typedef struct process_record_s {
	spinlock_t lock;
	tinyalloc_t tinyalloc;
	uint32_t main_thread;
	main_t main_func;
	env_t env;
	void *heap;
	uint32_t pid;
	struct process_record_s *next; // if hash collision
} process_record_t;

void *pmalloc(size_t size);
void pfree(void *ptr);

process_record_t *noza_process_self();
int noza_process_exec(main_t entry, int argc, char **argv); 
int noza_process_add_thread(process_record_t *process, uint32_t tid);
int noza_process_remove_thread(process_record_t *process, uint32_t tid);