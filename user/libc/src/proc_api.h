#pragma once

#include "type/dblist.h"
#include "tinyalloc_port/tinyalloc.h"
#include "type/hashslot.h"
#include "spinlock.h"
#include "kernel/noza_config.h"

typedef struct env_s {
	int argc;
	char *argv[];
} env_t;

typedef int (*main_t)(int argc, char **argv);

typedef struct process_record_s {
	spinlock_t lock;
	hash_item_t hash_item;
	tinyalloc_t tinyalloc;
	uint32_t main_thread;
	uint32_t thread_count;
	uint32_t child_thread[NOZA_PROC_THREAD_COUNT];
	main_t main_func;
	env_t *env;
	void *heap;
	struct process_record_s *next;
} process_record_t;

void *pmalloc(size_t size);
void pfree(void *ptr);

process_record_t *noza_process_self();
int noza_process_init();
int noza_process_exec(main_t entry, int argc, char **argv); 
int noza_process_add_thread(process_record_t *process, uint32_t tid);
int noza_process_remove_thread(process_record_t *process, uint32_t tid);
int noza_process_terminate_children_threads(process_record_t *process);
int noza_process_crt0(void *param, uint32_t tid);
