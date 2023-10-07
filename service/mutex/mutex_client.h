#pragma once

#include <stdint.h>

// the client api
typedef struct {
	uint32_t mid;
	uint32_t token;
} mutex_t;

int mutex_acquire(mutex_t *mutex);
int mutex_release(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_trylock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);