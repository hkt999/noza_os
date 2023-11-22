#include "sync_serv.h"
#include "sync_client.h"
#include "../name_lookup/name_lookup_client.h"
#include "nozaos.h"
#include <string.h>
#include <stdio.h>

// TODO: move sync_pid form "ask"
static uint32_t sync_vid = 0;
static inline void check_sync_vid()
{
	while (sync_vid == 0) {
		uint32_t value;
		if (name_lookup_search("noza_sync", &value) == 0) {
			sync_vid = value;
		} else {
			noza_thread_sleep_us(0, NULL); // yield
		}
	}
}

int mutex_acquire(mutex_t *mutex)
{
	check_sync_vid();
	mutex_msg_t msg = {.cmd = MUTEX_ACQUIRE, .mid = mutex->mid, .token = 0, .code = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
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
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_lock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_LOCK, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_trylock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_TRYLOCK, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int mutex_unlock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_UNLOCK, .mid = mutex->mid, .token = mutex->token };
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

// cond implementation

int cond_acquire(cond_t *cond)
{
	check_sync_vid();
	memset(cond, 0, sizeof(cond_t));
	cond_msg_t msg = {.cmd = COND_ACQUIRE};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
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
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int cond_wait(cond_t *cond, mutex_t *mutex)
{
	cond->mid = mutex->mid;
	cond->mtoken = mutex->token;
	cond_msg_t msg = {.cmd = COND_WAIT, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = mutex->mid, .mtoken = mutex->token};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

// TODO: implement this
int cond_timedwait(cond_t *cond, mutex_t *mutex, uint32_t us)
{
	cond->mid = mutex->mid;
	cond->mtoken = mutex->token;
	cond_msg_t msg = {.cmd = COND_TIMEDWAIT, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = mutex->mid, .mtoken = mutex->token};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int cond_signal(cond_t *cond)
{
	cond_msg_t msg = {.cmd = COND_SIGNAL, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = -1};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int cond_broadcast(cond_t *cond)
{
	cond_msg_t msg = {.cmd = COND_BROADCAST, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = -1};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

// semaphore
int semaphore_init(semaphore_t *sem, int value)
{
	check_sync_vid();
	sem_msg_t msg = {.cmd = SEM_ACQUIRE, .value = value, .code = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	if (msg.code == SEM_SUCCESS) {
		sem->sid = msg.sid;
		sem->stoken = msg.stoken;
	}
	return msg.code;
}

int semaphore_destroy(semaphore_t *sem)
{
	sem_msg_t msg = {.cmd = SEM_RELEASE, .sid = sem->sid, .stoken = sem->stoken, .code = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int semaphore_wait(semaphore_t *sem)
{
	sem_msg_t msg = {.cmd = SEM_WAIT, .sid = sem->sid, .stoken = sem->stoken, .code = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int semaphore_trywait(semaphore_t *sem)
{
	sem_msg_t msg = {.cmd = SEM_TRYWAIT, .sid = sem->sid, .stoken = sem->stoken, .code = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int semaphore_post(semaphore_t *sem)
{
	sem_msg_t msg = {.cmd = SEM_POST, .sid = sem->sid, .stoken = sem->stoken, .code = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	noza_call(&noza_msg);
	return msg.code;
}

int semaphore_getvalue(semaphore_t *sem, int *sval)
{
	sem_msg_t msg = {.cmd = SEM_GETVALUE, .sid = sem->sid, .stoken = sem->stoken, .code = 0, .value = 0};
	noza_msg_t noza_msg = {.to_vid = sync_vid, .ptr = (void *)&msg, .size = sizeof(msg)};
	if (noza_call(&noza_msg) == 0) {
		*sval = msg.value;
	}
	return msg.code;
}
