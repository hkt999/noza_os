#include "sync_serv.h"
#include "sync_client.h"
#include "../name_lookup/name_lookup_client.h"
#include "nozaos.h"
#include <string.h>
#include <stdio.h>

static uint32_t sync_service_id;
static uint32_t sync_vid;

static int resolve_sync_vid(uint32_t *resolved_vid)
{
	if (sync_vid != 0) {
		if (resolved_vid) {
			*resolved_vid = sync_vid;
		}
		return 0;
	}

	uint32_t vid = 0;
	int ret = name_lookup_resolve(NOZA_SYNC_SERVICE_NAME, &sync_service_id, &vid);
	if (ret == NAME_LOOKUP_OK) {
		sync_vid = vid;
		if (resolved_vid) {
			*resolved_vid = vid;
		}
		return 0;
	}

	return ret;
}

static void ensure_sync_vid(void)
{
	while (resolve_sync_vid(NULL) != 0) {
		noza_thread_sleep_us(0, NULL);
	}
}

static int sync_call(noza_msg_t *msg)
{
	ensure_sync_vid();
	msg->to_vid = sync_vid;
	int ret = noza_call(msg);
	if (ret != 0) {
		sync_vid = 0;
	}
	return ret;
}

int mutex_acquire(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_ACQUIRE, .mid = mutex->mid, .token = 0, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	if (ret != 0) {
		return ret;
	}
	if (msg.code == MUTEX_SUCCESS) {
		mutex->mid = msg.mid;
		mutex->token = msg.token;
	}
	return msg.code;
}

int mutex_release(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_RELEASE, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int mutex_lock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_LOCK, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int mutex_trylock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_TRYLOCK, .mid = mutex->mid, .token = mutex->token, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int mutex_unlock(mutex_t *mutex)
{
	mutex_msg_t msg = {.cmd = MUTEX_UNLOCK, .mid = mutex->mid, .token = mutex->token };
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

// cond implementation

int cond_acquire(cond_t *cond)
{
	memset(cond, 0, sizeof(cond_t));
	cond_msg_t msg = {.cmd = COND_ACQUIRE};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	if (ret != 0) {
		return ret;
	}
	if (msg.code == COND_SUCCESS) {
		cond->cid = msg.cid;
		cond->ctoken = msg.ctoken;
	}
	return msg.code;
}

int cond_release(cond_t *cond)
{
	cond_msg_t msg = {.cmd = COND_RELEASE, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int cond_wait(cond_t *cond, mutex_t *mutex)
{
	cond->mid = mutex->mid;
	cond->mtoken = mutex->token;
	cond_msg_t msg = {.cmd = COND_WAIT, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = mutex->mid, .mtoken = mutex->token};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

// TODO: implement this
int cond_timedwait(cond_t *cond, mutex_t *mutex, uint32_t us)
{
	(void)us;
	cond->mid = mutex->mid;
	cond->mtoken = mutex->token;
	cond_msg_t msg = {.cmd = COND_TIMEDWAIT, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = mutex->mid, .mtoken = mutex->token};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int cond_signal(cond_t *cond)
{
	cond_msg_t msg = {.cmd = COND_SIGNAL, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = -1};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int cond_broadcast(cond_t *cond)
{
	cond_msg_t msg = {.cmd = COND_BROADCAST, .cid = cond->cid, .ctoken = cond->ctoken, .code = 0, .mid = -1};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

// semaphore
int semaphore_init(semaphore_t *sem, int value)
{
	sem_msg_t msg = {.cmd = SEM_ACQUIRE, .value = value, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	if (ret != 0) {
		return ret;
	}
	if (msg.code == SEM_SUCCESS) {
		sem->sid = msg.sid;
		sem->stoken = msg.stoken;
	}
	return msg.code;
}

int semaphore_destroy(semaphore_t *sem)
{
	sem_msg_t msg = {.cmd = SEM_RELEASE, .sid = sem->sid, .stoken = sem->stoken, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int semaphore_wait(semaphore_t *sem)
{
	sem_msg_t msg = {.cmd = SEM_WAIT, .sid = sem->sid, .stoken = sem->stoken, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int semaphore_trywait(semaphore_t *sem)
{
	sem_msg_t msg = {.cmd = SEM_TRYWAIT, .sid = sem->sid, .stoken = sem->stoken, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int semaphore_post(semaphore_t *sem)
{
	sem_msg_t msg = {.cmd = SEM_POST, .sid = sem->sid, .stoken = sem->stoken, .code = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	return ret != 0 ? ret : msg.code;
}

int semaphore_getvalue(semaphore_t *sem, int *sval)
{
	sem_msg_t msg = {.cmd = SEM_GETVALUE, .sid = sem->sid, .stoken = sem->stoken, .code = 0, .value = 0};
	noza_msg_t noza_msg = {.ptr = (void *)&msg, .size = sizeof(msg)};
	int ret = sync_call(&noza_msg);
	if (ret == 0) {
		*sval = msg.value;
	}
	return ret != 0 ? ret : msg.code;
}
