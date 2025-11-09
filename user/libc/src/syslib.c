#include <stdio.h>
#include <stdlib.h> // TODO: remove malloc and free to use the memory service
#include "kernel/noza_config.h"
#include "setjmp.h"
#include "posix/errno.h"
#include "spinlock.h"
#include "nozaos.h"
#include "proc_api.h"
#include "thread_api.h"

extern void noza_thread_request_reserved_vid(uint32_t vid);

#define NO_AUTO_FREE_STACK	0
#define AUTO_FREE_STACK		1

extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
extern int __noza_futex_wait(uint32_t *addr, uint32_t expected, int32_t timeout_us);
extern int __noza_futex_wake(uint32_t *addr, uint32_t count);

inline static uint32_t *get_stack_ptr(uint32_t pid, uint32_t *size) {
	thread_record_t *record = get_thread_record(pid);
	if (record) {
		*size = record->stack_size;
		return record->stack_ptr;
	}

	return NULL;
}

// TODO: move this to platform/arch
uint32_t noza_get_stack_space() {
    uint32_t pid;
    if (noza_thread_self(&pid) == 0) {
        uint32_t sp;
        asm volatile ("mov %0, sp\n" : "=r" (sp));
		uint32_t stack_size;
        uint32_t stack_bottom = (uint32_t)get_stack_ptr(pid, &stack_size);
		return (stack_bottom + stack_size) - sp;
	} else {
		return 0; // fail
    }
}

typedef struct service_entry {
	int (*entry)(void *param, uint32_t pid);
	void *stack;
	uint32_t stack_size;
	uint32_t reserved_vid;
} service_entry_t;

static service_entry_t service_entry[NOZA_MAX_SERVICES];
static int service_count = 0;
void noza_add_service_with_vid(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size, uint32_t reserved_vid) {
    if (service_count >= NOZA_MAX_SERVICES) {
        printf("fatal: noza_add_service: too many services\n");
		return;
    }
    service_entry[service_count].entry = entry;
	service_entry[service_count].stack = stack;
	service_entry[service_count].stack_size = stack_size;
	service_entry[service_count].reserved_vid = reserved_vid;
	service_count++;
}

void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size)
{
	noza_add_service_with_vid(entry, stack, stack_size, NOZA_VID_AUTO);
}

#define SERVICE_PRIORITY    0
int service_main(int argc, char *argv[])
{   
    // initial all registered service
	if (service_count > 0) {
		for (int i = 0; i < service_count; i++) {
			uint32_t th;
			noza_thread_request_reserved_vid(service_entry[i].reserved_vid);
			noza_thread_create_with_stack(&th, service_entry[i].entry, NULL, SERVICE_PRIORITY,
				service_entry[i].stack, service_entry[i].stack_size, NO_AUTO_FREE_STACK);
		}
		//service_entry[i].entry(NULL, SERVICE_PRIORITY); // no return
	}
	while (1) { noza_thread_sleep_ms(1000, NULL); }

	// if there is no service, just return
	return -1;
}   


int noza_thread_sleep_us(int64_t us, int64_t *remain_us) {
	noza_time64_t tm;
	noza_time64_t remain;

	tm.high = (uint32_t)(us >> 32);
	tm.low = (uint32_t)(us & 0xFFFFFFFF);

	extern uint32_t __noza_thread_sleep(noza_time64_t *tm, noza_time64_t *remain); // in assembly
	uint32_t ret = __noza_thread_sleep(&tm, &remain);

	if (remain_us) {
		*remain_us = ((uint64_t)remain.high << 32) | remain.low;
	}

	return ret;
}

int noza_thread_sleep_ms(int64_t ms, int64_t *remain_ms) {
	int ret = noza_thread_sleep_us(ms * 1000, remain_ms);
	if (remain_ms) {
		*remain_ms /= 1000;
	}
	return ret;
}

int noza_set_errno(int errno) {
	uint32_t pid;
	if (noza_thread_self(&pid) == 0) {
		thread_record_t *record = get_thread_record(pid);
		if (record == NULL) {
			printf("fatal: noza_set_errno: pid %ld not found\n", pid);
			return -1; // TODO: return errno
		}
		record->errno = errno;
		return 0;
	}

	return -1;
}

int noza_errno() {
	uint32_t pid;
	if (noza_thread_self(&pid) == 0) {
		thread_record_t *th = get_thread_record(pid);
		if (th == NULL) {
			printf("fatal: noza_errno: pid %ld not found\n", pid);
			return -1; // TODO: return errno
		}
		return th->errno;
	}

	return -1;
}

int noza_thread_self(uint32_t *pid) {
	extern uint32_t NOZAOS_PID[NOZA_OS_NUM_CORES];
	uint32_t sp;
	asm volatile ("mov %0, sp\n" : "=r" (sp));
	for (int core = 0; core < NOZA_OS_NUM_CORES; core++) {
		uint32_t noza_pid = NOZAOS_PID[core];
		thread_record_t *thread_record = get_thread_record(noza_pid);
		if (thread_record) {
			if ((uint32_t)thread_record->stack_ptr <= sp && sp < (uint32_t)thread_record->stack_ptr + thread_record->stack_size) {
				*pid = noza_pid;
				return 0; // success
			}
		}
	}

	return -1;
}

int noza_futex_wait(uint32_t *addr, uint32_t expected, int32_t timeout_us)
{
	return __noza_futex_wait(addr, expected, timeout_us);
}

int noza_futex_wake(uint32_t *addr, uint32_t count)
{
	return __noza_futex_wake(addr, count);
}
