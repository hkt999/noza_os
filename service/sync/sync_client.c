#include "sync_serv.h"
#include "sync_client.h"
#include "nozaos.h"
#include <string.h>
#include <stdio.h>

// TODO: move mutex_pid form "ask"
extern uint32_t mutex_pid;
int mutex_acquire(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_ACQUIRE, .mid = mutex->mid, .token = 0, .code = 0};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = noza_call(&noza_msg); // TODO: check return value
	if (msg.code == MUTEX_SUCCESS) {
		mutex->mid = msg.mid;
		mutex->token = msg.token;
	}
	return msg.code;
}

int mutex_release(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_RELEASE, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_lock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_LOCK, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
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

// cond implementation

int cond_acquire(cond_t *cond)
{
	memset(cond, 0, sizeof(cond_t));
	cond_msg_t msg = {.cmd = COND_ACQUIRE};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	if (msg.code == COND_SUCCESS) {
		cond->cid = msg.cid;
		cond->ctoken = msg.ctoken;
	}
	return msg.code;
}

int cond_release(cond_t *cond)
{
	cond_msg_t msg = {.cmd = COND_RELEASE, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int cond_wait(cond_t *cond, mutex_t *mutex)
{
	cond->mid = mutex->mid;
	cond->mtoken = mutex->token;
	cond_msg_t msg = {.cmd = COND_WAIT, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = mutex->mid, .mtoken = mutex->token};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

// TODO: implement this
int cond_timedwait(cond_t *cond, mutex_t *mutex, uint32_t us)
{
	return -1;
}

int cond_signal(cond_t *cond)
{
	cond_msg_t msg = {.cmd = COND_SIGNAL, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = -1};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int cond_broadcast(cond_t *cond)
{
	cond_msg_t msg = {.cmd = COND_BROADCAST, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = -1};
	noza_msg_t noza_msg = {.to_pid = mutex_pid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}