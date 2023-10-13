#include <stdio.h>
#include "mutex_serv.h"
#include "mutex_client.h"
#include "nozaos.h"

// TODO: move mutex_pid form "ask"
extern uint32_t mutex_pid;
int mutex_acquire(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_ACQUIRE, .mid = mutex->mid, .token = 0, .code = 0};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = noza_call(&noza_msg);
	if (msg.code == MUTEX_SUCCESS) {
		mutex->mid = msg.mid;
		mutex->token = msg.token;
	}
	printf("acquire mutex->mid=%d, mutex->token=%d, code=%d\n", mutex->mid, mutex->token, msg.code);
	return msg.code;
}

int mutex_release(mutex_t *mutex)
{
	printf("release mutex->mid=%d, mutex->token=%d\n", mutex->mid, mutex->token);
	mutex_msg_t msg = {.cmd = MUTEX_RELEASE, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_lock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_LOCK, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	printf("enter mutex_lock\n");
	noza_call(&noza_msg);
	printf("leave mutex_lock\n");
	return msg.code;
}

int mutex_trylock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_TRYLOCK, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_unlock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_UNLOCK, .mid = mutex->mid, .token = mutex->token };
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}