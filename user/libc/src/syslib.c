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

typedef struct thread_info {
	int (*user_entry)(void *param, uint32_t pid);
	void *user_param;
	uint32_t *stack_ptr;
	uint32_t stack_size;
	uint32_t priority;
	uint32_t need_free_stack;
	uint32_t pid;
	jmp_buf jmp_buf;
	uint32_t errno;
} thread_info_t;
static thread_info_t *THREAD_INFO[NOZA_OS_TASK_LIMIT] = {0}; // user thread information

extern void app_run(thread_info_t *info);

#define MAX_SERVICES    8
typedef struct service_entry {
	int (*entry)(void *param, uint32_t pid);
	void *stack;
	uint32_t stack_size;
} service_entry_t;

//static void *service_entry[MAX_SERVICES];
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

uint32_t save_exit_context(thread_info_t *thread_info, uint32_t pid)
{
	if (thread_info->pid != pid) {
		// not setup yet, just setup
		thread_info->pid = pid;
		THREAD_INFO[pid] = thread_info;
	}

	return setjmp(thread_info->jmp_buf);
}

void free_stack(uint32_t pid)
{
	// sanity check
	if (pid >= NOZA_OS_TASK_LIMIT) {
		printf("fatal: free_stack: invalid pid: %ld\n", pid);
		return;
	}
	if (THREAD_INFO[pid] == NULL) {
		printf("fatal: free_stack: pid %ld not found\n", pid);
		return;
	}
	if (THREAD_INFO[pid]->stack_ptr == NULL) {
		printf("fatal: free_stack: pid %ld stack_ptr is NULL\n", pid);
		return;
	}
	// free user level stack
	if (THREAD_INFO[pid]->need_free_stack == AUTO_FREE_STACK) {
		noza_free(THREAD_INFO[pid]->stack_ptr);
	}
	THREAD_INFO[pid] = NULL; // clear
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
	thread_info_t *thread_info = (thread_info_t *)user_stack; // stack tail for temporary use
	thread_info->user_entry = entry;
	thread_info->user_param = param;
	thread_info->stack_ptr = (uint32_t *)user_stack;
	thread_info->stack_size = size;
	thread_info->priority = priority;
	thread_info->need_free_stack = auto_free_stack;
	thread_info->pid = -1;
	thread_info->errno = 0;

	// setup registers for system call
	info.r0 = NSC_THREAD_CREATE;
	info.r1 = (uint32_t)app_run;
	info.r2 = (uint32_t)thread_info;
	info.r3 = priority;
	extern void noza_thread_create_primitive(create_thread_t *info);
	noza_thread_create_primitive(&info);
	thread_info->pid = info.r1;
	*pth = info.r1;
	THREAD_INFO[info.r1] = thread_info;

	return info.r0;
}

//#define DEFAULT_STACK_SIZE 4096 // TODO: move this flag to config.h
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
	longjmp(THREAD_INFO[pid]->jmp_buf, exit_code);
}

int noza_thread_sleep_us(int64_t us, int64_t *remain_us)
{
	uint32_t r0 = (uint32_t)(us >> 32);
	uint32_t r1 = (uint32_t)(us & 0xFFFFFFFF);
	uint32_t r2;

	extern void noza_thread_sleep(uint32_t r0, uint32_t r1); // in assembly
	noza_thread_sleep(r0, r1);
	__asm__ volatile(
		"mov %0, r0\n"  // return code
		"mov %1, r1\n"  // high 32bits
		"mov %2, r2\n"  // low 32bits
		: "=r" (r0), "=r" (r1), "=r" (r2)
		: 
		: "memory"
	);

	if (remain_us) {
		*remain_us = ((uint64_t)r1 << 32) | r2;
	}
	return r0;
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
		THREAD_INFO[pid]->errno = errno;
		return 0;
	}

	return -1;
}

int noza_get_errno()
{
	uint32_t pid;
	if (noza_thread_self(&pid) == 0) {
		return THREAD_INFO[pid]->errno;
	}

	return -1;
}