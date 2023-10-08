#pragma once

#include <stdint.h>
#include <stddef.h>
#include "errno.h"
#include <service/mutex/mutex_client.h>

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define SCHED_RR    0
#define SCHED_FIFO  1
#define SCHED_OTHER 2

#define PTHREAD_INHERIT_SCHED   0
#define PTHREAD_EXPLICIT_SCHED  1

#define PTHREAD_SCOPE_SYSTEM    0
#define PTHREAD_SCOPE_PROCESS   1

typedef mutex_t pthread_mutex_t;

struct sched_param {
    int sched_priority;
};

typedef struct {
} pthread_mutexattr_t;

typedef struct {
    uint32_t policy:4;
    uint32_t inheritsched:4;
    uint32_t detachstate:4;
    uint32_t scope:4;

    uint32_t stacksize;
    uint32_t guardsize;
    void *stackaddr;
    struct sched_param schedparam;
} pthread_attr_t;

typedef struct {
	uint32_t id;
    void *(*start_routine)(void *);
    void *arg;
} pthread_t;

// pthread primitives
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
int pthread_exit(void *retval);
int pthread_detach(pthread_t thread);
int pthread_yield(void);
int pthread_equal(pthread_t t1, pthread_t t2);

// attr
// initialize and destroy thread attributes object
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);

// set and get the detach state attribute 
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);

// set and get the stack size attribute 
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);

// set and get the stack address attribute (deprecated in some systems)
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr);
int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr);

// set and get the stack attribute
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize);

// set and get the guard size attribute
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize);
int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize);

// set and get the scheduling parameter attributes
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param);

// set and get the scheduling policy attribute 
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy);

// set and get the inherit scheduling attribute 
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched);
int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched);

// set and get the scope attribute
int pthread_attr_setscope(pthread_attr_t *attr, int scope);
int pthread_attr_getscope(const pthread_attr_t *attr, int *scope);

// mutex
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock (pthread_mutex_t *mutex);
int pthread_mutex_trylock (pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

// not supported
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));