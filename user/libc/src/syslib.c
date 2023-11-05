#include <stdio.h>
#include <stdlib.h> // TODO: remove malloc and free to use the memory service
#include "nozaos.h"
#include "kernel/syscall.h"
#include "kernel/noza_config.h"
#include "setjmp.h"
#include "posix/errno.h"
#include "service/memory/mem_client.h"

#define NO_AUTO_FREE_STACK	0
#define AUTO_FREE_STACK		1

extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

typedef struct {
	int (*user_entry)(void *param, uint32_t pid);
	void *user_param;
	uint32_t *stack_ptr;
	uint32_t stack_size;
	uint32_t priority;
	uint32_t need_free_stack;
	uint32_t pid;
	void *thread_data;
	jmp_buf jmp_buf;
	uint32_t errno;
} thread_record_t;

static thread_record_t *THREAD_RECORD[NOZA_OS_TASK_LIMIT] = {0}; // user thread information

uint32_t *get_stack_ptr(uint32_t pid)
{
	return THREAD_RECORD[pid]->stack_ptr;
}

uint32_t noza_get_stack_space() {
    uint32_t pid;
    if (noza_thread_self(&pid) == 0) {
        uint32_t sp;
        asm volatile ("mov %0, sp\n" : "=r" (sp));
        uint32_t stack_bottom = (uint32_t)THREAD_RECORD[pid]->stack_ptr;
        uint32_t stack_size = THREAD_RECORD[pid]->stack_size;
		return (stack_bottom + stack_size) - sp;
	} else {
		return 0; // fail
    }
}

extern void app_run(thread_record_t *info);

#define MAX_SERVICES    8
typedef struct service_entry {
	int (*entry)(void *param, uint32_t pid);
	void *stack;
	uint32_t stack_size;
} service_entry_t;

static service_entry_t service_entry[MAX_SERVICES];
static int service_count = 0;
void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size)
{
    if (service_count >= MAX_SERVICES) {
        printf("fatal: noza_add_service: too many services\n");
		return;
    }
    service_entry[service_count].entry = entry;
	service_entry[service_count].stack = stack;
	service_entry[service_count].stack_size = stack_size;
	service_count++;
}

#define SERVICE_PRIORITY	0
void noza_run_services()
{
    // initial all registered service
    for (int i = 0; i < service_count; i++) {
		uint32_t th;
		noza_thread_create_with_stack(&th, service_entry[i].entry, NULL, SERVICE_PRIORITY,
			service_entry[i].stack, service_entry[i].stack_size, NO_AUTO_FREE_STACK);
    }
}

uint32_t save_exit_context(thread_record_t *thread_record, uint32_t pid)
{
	if (thread_record->pid != pid) {
		// not setup yet, just setup
		thread_record->pid = pid;
		THREAD_RECORD[pid] = thread_record;
	}

	return setjmp(thread_record->jmp_buf);
}

uint32_t free_stack(uint32_t pid, uint32_t code)
{
	// sanity check
	if (pid >= NOZA_OS_TASK_LIMIT) {
		printf("fatal: free_stack: invalid pid: %ld\n", pid);
		return code;
	}
	if (THREAD_RECORD[pid] == NULL) {
		printf("fatal: free_stack: pid %ld not found\n", pid);
		return code;
	}
	if (THREAD_RECORD[pid]->stack_ptr == NULL) {
		printf("fatal: free_stack: pid %ld stack_ptr is NULL\n", pid);
		return code;
	}
	// free user level stack
	if (THREAD_RECORD[pid]->need_free_stack == AUTO_FREE_STACK) {
		noza_free(THREAD_RECORD[pid]->stack_ptr);
	}
	THREAD_RECORD[pid] = NULL; // clear

	return code;
}

typedef struct {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
} create_thread_t;

int noza_thread_create_with_stack(uint32_t *pth, int (*entry)(void *, uint32_t pid), void *param, uint32_t priority, void *user_stack, uint32_t size, uint32_t auto_free_stack)
{
	create_thread_t info;
	thread_record_t *thread_record = (thread_record_t *)user_stack; // stack tail for temporary use
	thread_record->user_entry = entry;
	thread_record->user_param = param;
	thread_record->stack_ptr = (uint32_t *)user_stack;
	thread_record->stack_size = size;
	thread_record->priority = priority;
	thread_record->need_free_stack = auto_free_stack;
	thread_record->pid = -1;
	thread_record->errno = 0;

	// setup register for system call
	info.r0 = NSC_THREAD_CREATE;
	info.r1 = (uint32_t)app_run;
	info.r2 = (uint32_t)thread_record;
	info.r3 = (uint32_t)priority;
	extern void noza_thread_create_primitive(create_thread_t *info);
	noza_thread_create_primitive(&info);
	thread_record->pid = info.r1;
	*pth = info.r1;
	THREAD_RECORD[info.r1] = thread_record;

	return info.r0;
}

int noza_thread_create(uint32_t *pth, int (*entry)(void *, uint32_t), void *param, uint32_t priority, uint32_t stack_size)
{
	uint8_t *stack_ptr = (uint8_t *)noza_malloc(stack_size);
	if (stack_ptr == NULL) {
		return EAGAIN;
	}
	return noza_thread_create_with_stack(pth, entry, param, priority, stack_ptr, stack_size, AUTO_FREE_STACK);
}

void noza_thread_exit(uint32_t exit_code)
{
	uint32_t pid;
	noza_thread_self(&pid);
	longjmp(THREAD_RECORD[pid]->jmp_buf, exit_code);
}

typedef struct time64_s {
	uint32_t high;
	uint32_t low;
} time64_t;

int noza_thread_sleep_us(int64_t us, int64_t *remain_us)
{
	time64_t tm;
	time64_t remain;

	tm.high = (uint32_t)(us >> 32);
	tm.low = (uint32_t)(us & 0xFFFFFFFF);

	extern uint32_t __noza_thread_sleep(time64_t *tm, time64_t *remain); // in assembly
	uint32_t ret = __noza_thread_sleep(&tm, &remain);

	if (remain_us) {
		*remain_us = ((uint64_t)remain.high << 32) | remain.low;
	}

	return ret;
}

int noza_thread_sleep_ms(int64_t ms, int64_t *remain_ms)
{
	int ret = noza_thread_sleep_us(ms * 1000, remain_ms);
	if (remain_ms) {
		*remain_ms /= 1000;
	}
	return ret;
}

int noza_set_errno(int errno)
{
	uint32_t pid;
	if (noza_thread_self(&pid) == 0) {
		THREAD_RECORD[pid]->errno = errno;
		return 0;
	}

	return -1;
}

int noza_errno()
{
	uint32_t pid;
	if (noza_thread_self(&pid) == 0) {
		return THREAD_RECORD[pid]->errno;
	}

	return -1;
}

int noza_thread_self(uint32_t *pid)
{
	extern uint32_t NOZAOS_PID[NOZA_OS_NUM_CORES];

	uint32_t sp;
	asm volatile ("mov %0, sp\n" : "=r" (sp));
	for (int core = 0; core < NOZA_OS_NUM_CORES; core++) {
		uint32_t noza_pid = NOZAOS_PID[core];
		thread_record_t *th = THREAD_RECORD[noza_pid];
		if (sp > (uint32_t )th->stack_ptr && sp < (uint32_t)th->stack_ptr + th->stack_size) {
			*pid = noza_pid;
			return 0; // success
		}
	}

	return -1;
}
