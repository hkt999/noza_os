#include <stdio.h>
#include <string.h>
#include "proc_api.h"
#include "spinlock.h"
#include "nozaos.h"
#include "thread_api.h"
#include "service/memory/mem_client.h"

static spinlock_t process_record_lock;
static process_record_t PROCESS_SLOT[NUM_PROCESS];
static process_record_t *process_head = NULL;

#define HASH_TABLE_SIZE		256
static spinlock_t process_record_lock;
static process_record_t *PROCESS_RECORD_HASH[HASH_TABLE_SIZE]; // hash table for thread record -- need protect !!
inline static uint8_t hash32to8(uint32_t value) {
    uint8_t byte1 = (value >> 24) & 0xFF; // High byte
    uint8_t byte2 = (value >> 16) & 0xFF; // Mid-high byte
    uint8_t byte3 = (value >> 8) & 0xFF;  // Mid-low byte
    uint8_t byte4 = value & 0xFF;         // Low byte

    // XOR the chunks together
    return (byte1 ^ byte2 ^ byte3 ^ byte4);
}

inline static void process_insert_to_mapping(process_record_t *process_record)
{
    int hash = hash32to8(process_record->pid);
    noza_raw_lock(&process_record_lock);
    process_record->next = PROCESS_RECORD_HASH[hash];
    PROCESS_RECORD_HASH[hash] = process_record;
    noza_spinlock_unlock(&process_record_lock);
}

inline static void thread_remove_from_mapping(uint32_t pid)
{
    int hash = hash32to8(pid);
    noza_raw_lock(&process_record_lock);
    process_record_t *prev = NULL;
    process_record_t *current = PROCESS_RECORD_HASH[hash];
    while (current) {
        if (current->pid == pid) {
            if (prev) {
                prev->next = current->next;
            } else {
                PROCESS_RECORD_HASH[hash] = current->next;
            }
            break;
        }
        prev = current;
        current = current->next;
    }
    noza_spinlock_unlock(&process_record_lock);
}

process_record_t *get_process_record(uint32_t pid)
{
	int hash = hash32to8(pid);
	noza_raw_lock(&process_record_lock);
	process_record_t *process_record = PROCESS_RECORD_HASH[hash];
	while (process_record) {
		if (process_record->pid == pid)
			break;

		process_record = process_record->next;
	}
	noza_spinlock_unlock(&process_record_lock);
	return process_record;
}

////////////////////

void *pmalloc(size_t size)
{
	uint32_t tid;
	if (noza_thread_self(&tid) == 0) {
		thread_record_t *thread_record = get_thread_record(tid);
		if (thread_record) {
			process_record_t *process = thread_record->process;
			if (process) {
				noza_spinlock_lock(&process->lock);
				void *ptr = ta_alloc(&process->tinyalloc, size);
				noza_spinlock_unlock(&process->lock);
				return ptr;
			} else {
				// unlikely to be here, TODO: exception
			}
		} else {
			// unlikely to be here, TODO: exception
		}
	}
	return NULL;
}

void pfree(void *ptr) 
{
	uint32_t tid;
	if (noza_thread_self(&tid) == 0) {
		thread_record_t *thread_record = get_thread_record(tid);
		if (thread_record) {
			process_record_t *process = get_thread_record(tid)->process;
			if (process) {
				noza_spinlock_lock(&process->lock);
				ta_free(&process->tinyalloc, ptr);
				noza_spinlock_unlock(&process->lock);
			} else {
				// unlikely to be here, TODO: exception
			}
		} else {
			// unlikely to be here, TODO: exception
		}
	}
}

static int process_crt0(void *param, uint32_t tid)
{
	uint32_t pid;
	process_record_t *process = (process_record_t *)param;
	if (noza_thread_self(&tid) != 0) {
		// unlikely to be here
		printf("fatal: process_crt0: noza_thread_self failed\n");
		return -1;

	}
	thread_record_t *main_thread = get_thread_record(tid);
	if (main_thread == NULL) {
		// unlikely to be here
		printf("fatal: process_crt0: get_thread_record failed\n");
		return -1;
	}
	main_thread->process = process;
	process->heap = noza_malloc(1024); // TODO: use a customized size heap

	noza_spinlock_init(&process->lock);
	ta_init(&process->tinyalloc, process->heap, process->heap + 1024, 256, 16, 8);
	int ret =  process->main_func(process->env.argc, process->env.argv);
	noza_free(process->heap);
	noza_spinlock_free(&process->lock);

	return ret;
}

void noza_root_process()
{
	//DBLIST_INIT(PROCESS_SLOT, NUM_PROCESS)
	//process_head = &PROCESS_SLOT[0];
}

int noza_process_exec(main_t entry, int argc, char **argv) {
	uint32_t tid = 0;
	if (process_head == NULL) {
		printf("fatal: process_create: no more process slot\n");
		return -1;
	}

	process_record_t *process = process_head;
	//process_head = (process_record_t *)dblist_remove_head(&process_head->link);
	memset(process, 0, sizeof(process_record_t));
	process->main_func = entry;
	noza_thread_create(&tid, process_crt0, (void *)process, 0, 1024);  // TODO: consider the stack size
}

process_record_t *noza_process_self() // TODO: return process is instead of process_t
{
	uint32_t tid;
	if (noza_thread_self(&tid) == 0) {
		thread_record_t *thread_record = get_thread_record(tid);
		if (thread_record) {
			return thread_record->process;
		} else {
			// unlikely to be here, TODO: exception
		}
	}

	// unlikely to be here 
	return NULL;
}

// called from app_run.S
// thread_record points to the stack tail
uint32_t save_exit_context(thread_record_t *thread_record, uint32_t pid)
{
	return setjmp(thread_record->jmp_buf);
}

int noza_process_add_thread(process_record_t *process, uint32_t tid)
{
	return 0;
}

int noza_process_remove_thread(process_record_t *process, uint32_t tid)
{
	return 0;
}