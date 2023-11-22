#include <string.h>
#include "service/memory/mem_client.h"
#include "proc_api.h"
#include "thread_api.h"

void *nz_malloc(size_t size)
{
	uint32_t tid;
	if (noza_thread_self(&tid) == 0) {
		thread_record_t *thread_record = get_thread_record(tid);
		if (thread_record) {
			process_record_t *process = thread_record->process;
			if (process) {
				noza_spinlock_lock(&process->lock);
				if (process->heap == NULL) {
					process->heap = noza_malloc(NOZA_PROCESS_HEAP_SIZE); // a little big danger, hold and wait
					ta_init(&process->tinyalloc, process->heap, process->heap + NOZA_PROCESS_HEAP_SIZE, 256, 16, 8);
				}
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

void nz_free(void *ptr) 
{
	uint32_t tid;
	if (noza_thread_self(&tid) == 0) {
		thread_record_t *thread_record = get_thread_record(tid);
		if (thread_record) {
			process_record_t *process = thread_record->process;
			if (process) {
				noza_spinlock_lock(&process->lock);
				if (process->heap) { // the heap is already allocated
					ta_free(&process->tinyalloc, ptr);
				}
				noza_spinlock_unlock(&process->lock);
			} else {
				// unlikely to be here, TODO: exception
			}
		} else {
			// unlikely to be here, TODO: exception
		}
	}
}

char *nz_strdup(char *string)
{
    char *buf = nz_malloc(strlen(string)+1);
    if (buf) {
        strcpy(buf, string);
        return buf;
    }
    return NULL;
}