#pragma once

#include <stdint.h>
#include <service/mutex/mutex_client.h>

typedef mutex_t pthread_mutex_t;
typedef struct {
} pthread_mutexattr_t;

typedef struct {
} pthread_attr_t;


typedef struct {
	uint32_t id;
    void *(*start_routine)(void *);
    void *arg;
} pthread_t;

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock (pthread_mutex_t *mutex);
int pthread_mutex_trylock (pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
int pthread_exit(void *retval);
int pthread_detach(pthread_t thread);
void pthread_yield(void);
int pthread_equal(pthread_t t1, pthread_t t2);