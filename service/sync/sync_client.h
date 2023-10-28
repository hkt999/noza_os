#pragma once

#include <stdint.h>

// the client api
typedef struct {
	uint32_t mid;
	uint32_t token;
} mutex_t;

typedef struct {
	uint32_t cid;
	uint32_t ctoken;
	uint32_t mid;
	uint32_t mtoken;
} cond_t;

typedef struct {
	uint32_t sid;
	uint32_t stoken;
} semaphore_t;

int mutex_acquire(mutex_t *mutex);
int mutex_release(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_trylock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);

int cond_acquire(cond_t *cond);
int cond_release(cond_t *cond);
int cond_wait(cond_t *cond, mutex_t *mutex);
int cond_timedwait(cond_t *cond, mutex_t *mutex, uint32_t us);
int cond_signal(cond_t *cond);
int cond_broadcast(cond_t *cond);

int semaphore_init(semaphore_t *sem, int value);
int semaphore_destroy(semaphore_t *sem);
int semaphore_wait(semaphore_t *sem);
int semaphore_trywait(semaphore_t *sem);
int semaphore_post(semaphore_t *sem);
int semaphore_getvalue(semaphore_t *sem, int *sval);
