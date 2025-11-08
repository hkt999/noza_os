#pragma once

#include "type/dblist.h"
#if NOZA_PROCESS_USE_TLSF
#include "tlsf_port/tlsf.h"
#else
#include "tinyalloc_port/tinyalloc.h"
#endif
#include "type/hashslot.h"
#include "spinlock.h"
#include "kernel/noza_config.h"
#include "nozaos.h"

typedef struct env_s {
	int argc;
	char *argv[];
} env_t;

typedef struct process_record_s {
	spinlock_t lock;
	hash_item_t hash_item;
#if NOZA_PROCESS_USE_TLSF
	tlsf_t tlsf;
#else
	tinyalloc_t tinyalloc;
#endif
	main_t entry;
	uint32_t main_thread;
	uint32_t thread_count;
	uint32_t child_thread[NOZA_PROC_THREAD_COUNT];
	env_t *env;
	uint8_t env_buf[NOZA_PROCESS_ENV_SIZE];
	void *heap;
	struct process_record_s *next;
} process_record_t;

void *pmalloc(size_t size);
void pfree(void *ptr);

process_record_t *noza_process_self();
int noza_process_init();
int noza_process_add_thread(process_record_t *process, uint32_t tid);
int noza_process_remove_thread(process_record_t *process, uint32_t tid);
int noza_process_terminate_children_threads(process_record_t *process);
int noza_process_crt0(void *param, uint32_t tid);
