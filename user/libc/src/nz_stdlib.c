#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "service/memory/mem_client.h"
#include "proc_api.h"
#include "thread_api.h"

#if NOZA_PROCESS_USE_TLSF
static const char *process_name(process_record_t *process)
{
	if (process && process->env && process->env->argc > 0) {
		return process->env->argv[0];
	}
	return "?";
}
#define TLSF_LOG(fmt, ...) \
	printf("[tlsf] " fmt "\n", ##__VA_ARGS__)
static inline void tlsf_dump_range(process_record_t *process)
{
	printf("[tlsf] heap=%p..%p proc=%s\n",
		process->heap,
		(uint8_t *)process->heap + NOZA_PROCESS_HEAP_SIZE,
		process_name(process));
}
#else
#define TLSF_LOG(...)
#define tlsf_dump_range(x) ((void)0)
#endif

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
					if (process->heap) {
#if NOZA_PROCESS_USE_TLSF
						process->tlsf = tlsf_create_with_pool(process->heap, NOZA_PROCESS_HEAP_SIZE);
						if (process->tlsf == NULL) {
							noza_free(process->heap);
							process->heap = NULL;
							TLSF_LOG("pool init failed (proc=%s)", process_name(process));
						} else {
							tlsf_dump_range(process);
						}
#else
						ta_init(&process->tinyalloc, process->heap, process->heap + NOZA_PROCESS_HEAP_SIZE, 256, 16, 8);
#endif
					}
				}
				void *ptr = NULL;
				if (process->heap) {
#if NOZA_PROCESS_USE_TLSF
					ptr = tlsf_malloc(process->tlsf, size);
					if (ptr == NULL) {
						TLSF_LOG("alloc failed size=%zu (proc=%s)", size, process_name(process));
					} else {
						TLSF_LOG("alloc ptr=%p size=%zu (proc=%s)", ptr, size, process_name(process));
					}
#else
					ptr = ta_alloc(&process->tinyalloc, size);
#endif
				}
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
#if NOZA_PROCESS_USE_TLSF
					if (ptr) {
						uintptr_t base = (uintptr_t)process->heap;
						uintptr_t end = base + NOZA_PROCESS_HEAP_SIZE;
						uintptr_t addr = (uintptr_t)ptr;
						if (addr < base || addr >= end) {
							TLSF_LOG("free out of range ptr=%p (proc=%s)", ptr, process_name(process));
							noza_spinlock_unlock(&process->lock);
							return;
						}
					}
					tlsf_free(process->tlsf, ptr);
					TLSF_LOG("free ptr=%p (proc=%s)", ptr, process_name(process));
#else
					ta_free(&process->tinyalloc, ptr);
#endif
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
    if (buf == NULL) {
#if NOZA_PROCESS_USE_TLSF
		TLSF_LOG("strdup failed len=%zu", strlen(string));
#endif
        return NULL;
    }
    strcpy(buf, string);
    return buf;
}
