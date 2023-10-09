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

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED  1

#define PTHREAD_MUTEX_DEFAULT       0
#define PTHREAD_MUTEX_NORMAL        1
#define PTHREAD_MUTEX_ERRORCHECK    2
#define PTHREAD_MUTEX_RECURSIVE     3
#define NUM_MUTEX_TYPE              4

typedef struct {
    uint32_t shared:8;
    uint32_t type:8;
} pthread_mutexattr_t;

typedef struct {
    uint32_t policy:8;
    uint32_t inheritsched:8;
    uint32_t detachstate:8;
    uint32_t scope:8;

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
void pthread_exit(void *retval);
int pthread_detach(pthread_t thread);
int pthread_yield(void);
pthread_t pthread_self(void);
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
// mutex initialization
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);

// destroy the mutex
int pthread_mutex_destroy(pthread_mutex_t *mutex);

// lock the mutex
int pthread_mutex_lock(pthread_mutex_t *mutex);

// try to lock the mutex
int pthread_mutex_trylock(pthread_mutex_t *mutex);

// unlock the mutex
int pthread_mutex_unlock(pthread_mutex_t *mutex);

// initialize a mutex attributes object
int pthread_mutexattr_init(pthread_mutexattr_t *attr);

// destroy a mutex attributes object
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);

// set and get the process-shared attribute
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);

// set and get the type attribute (e.g., PTHREAD_MUTEX_NORMAL)
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

///////////////////////////////////////////////////////////
// not supported
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));