#include <stdio.h>
#include <stdlib.h> // TODO: remove malloc and free to use the memory service
#include "syslib.h"
#include "kernel/syscall.h"

extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

int noza_thread_yield()
{
	return noza_syscall(NSC_YIELD, 0, 0, 0);
}

int noza_thread_join(uint32_t thread_id)
{
	return noza_syscall(NSC_THREAD_JOIN, thread_id, 0, 0);
}

int noza_thread_sleep(uint32_t ms)
{
	return noza_syscall(NSC_SLEEP, ms, 0, 0);
}

typedef struct boot_info {
	void (*user_entry)(void *param);
	void *user_param;
	uint32_t *stack_ptr;
	uint32_t stack_size;
	uint32_t created;
} boot_info_t;

void free_stack(void *ptr)
{
	free(ptr);
}

extern void app_bootstrap(boot_info_t *info);
int noza_thread_create(void (*entry)(void *), void *param, uint32_t priority)
{
	volatile boot_info_t boot_info;
	boot_info.user_entry = entry;
	boot_info.user_param = param;
	boot_info.stack_ptr = (uint32_t *)malloc(1024); // TODO: use noza memory allocator
	boot_info.stack_size = 1024;
	boot_info.created = 0;
	printf("stack_ptr: %p\n", boot_info.stack_ptr);
    int ret =  noza_syscall(NSC_THREAD_CREATE, (uint32_t) app_bootstrap, (uint32_t) &boot_info, priority);
	while (boot_info.created==0) {
		noza_thread_yield();
	}

	return ret;
}

int noza_thread_change_priority(uint32_t thread_id, uint32_t priority)
{
	return noza_syscall(NSC_THREAD_CHANGE_PRIORITY, thread_id, priority, 0);
}

void noza_thread_terminate()
{
	noza_syscall(NSC_THREAD_TERMINATE, 0, 0, 0);
}

int noza_recv(noza_msg_t *msg)
{
	uint32_t ret;
	noza_syscall(NSC_RECV, 0, 0, 0);

	asm volatile (
		"mov %[ret],  r0\n\t"
		"mov %[pid],  r1\n\t"
		"mov %[ptr],  r2\n\t"
		"mov %[size], r3\n\t"
		: [ret] "=r" (ret), [pid] "=r" (msg->pid), [ptr] "=r" (msg->ptr), [size] "=r" (msg->size)
	);
	return ret; // success
}

int noza_reply(noza_msg_t *msg)
{
	return noza_syscall(NSC_REPLY, msg->pid, (uint32_t)msg->ptr, msg->size);
}

int noza_call(noza_msg_t *msg)
{
	int code;
	noza_syscall(NSC_CALL, msg->pid, (uint32_t)msg->ptr, msg->size);

	asm volatile (
		"mov %[code], r0\n\t"
		"mov %[pid],  r1\n\t"
		"mov %[ptr],  r2\n\t"
		"mov %[size], r3\n\t"
		: [code] "=r" (code), [pid] "=r" (msg->pid), [ptr] "=r" (msg->ptr), [size] "=r" (msg->size)
	);
	return code;
}

int noza_nonblock_call(uint32_t pid, noza_msg_t *msg)
{
	return noza_syscall(NSC_NB_CALL, msg->pid, (uint32_t)msg->ptr, msg->size);
}

int noza_nonblock_recv(uint32_t pid, noza_msg_t *msg)
{
	return noza_syscall(NSC_NB_RECV, msg->pid, (uint32_t)msg->ptr, msg->size);
}
