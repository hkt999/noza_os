#include <stdio.h>
#include "mutex_serv.h"
#include "mutex_client.h"
#include "nozaos.h"


extern uint32_t mutex_pid;
int mutex_acquire(mutex_t *mutex)
{
	printf("mutex_pid=%d\n", mutex_pid);
	mutex_msg_t msg = {.cmd = MUTEX_ACQUIRE };
	noza_msg_t noza_msg = {.pid = 0, .ptr = (void *)&msg, .size = sizeof(msg)};
	printf("0, mutex_acquire: %d\n", sizeof(msg));
	noza_call(&noza_msg);
	printf("1, mutex_acquire: %d\n", sizeof(msg));
	if (msg.code == MUTEX_SUCCESS) {
		mutex->mid = msg.mid;
		mutex->token = msg.token;
	}
	return msg.code;
}

int mutex_release(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_RELEASE, .mid = mutex->mid, .token = mutex->token };
	noza_msg_t noza_msg = {.pid = 0, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_lock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_LOCK, .mid = mutex->mid, .token = mutex->token };
	noza_msg_t noza_msg = {.pid = 0, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_trylock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_TRYLOCK, .mid = mutex->mid, .token = mutex->token };
	noza_msg_t noza_msg = {.pid = 0, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_unlock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_UNLOCK, .mid = mutex->mid, .token = mutex->token };
	noza_msg_t noza_msg = {.pid = 0, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}