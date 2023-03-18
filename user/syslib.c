#include "syslib.h"
#include "kernel/syscall.h"

extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

int noza_thread_yield()
{
	return noza_syscall(NSC_YIELD, 0, 0, 0);
}

int noza_thread_sleep(uint32_t ms)
{
	return noza_syscall(NSC_SLEEP, ms, 0, 0);
}

int noza_thread_create(void (*entry)(void *), void *param, uint32_t priority)
{
    return noza_syscall(NSC_THREAD_CREATE, (uint32_t) entry, (uint32_t) param, priority);
}

int noza_thread_change_priority(uint32_t thread_id, uint32_t priority)
{
	return noza_syscall(NSC_THREAD_CHANGE_PRIORITY, thread_id, priority, 0);
}

void noza_thread_terminate()
{
	noza_syscall(NSC_THREAD_TERMINATE, 0, 0, 0);
}

int noza_recv(noza_port_t *port, noza_msg_t *msg)
{
	return 0;
}

int noza_reply(noza_port_t *port, uint32_t reply_code)
{
	return 0;
}

int noza_call(noza_port_t *port, noza_msg_t *msg)
{
	return 0;
}

int noza_nonblock_call(noza_port_t *port, noza_msg_t *msg)
{
	return 0;
}

int noza_nonblock_recv(noza_port_t *port, noza_msg_t *msg)
{
	return 0;
}