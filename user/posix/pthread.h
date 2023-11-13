#pragma once

#include <stdint.h>
#include <stddef.h>
#include "errno.h"
#include "noza_time.h"
#include "spinlock.h"
#include <service/sync/sync_client.h>

#define NZ_PTHREAD_CREATE_JOINABLE 0
#define NZ_PTHREAD_CREATE_DETACHED 1

#define NZ_SCHED_RR    0
#define NZ_SCHED_FIFO  1
#define NZ_SCHED_OTHER 2

#define NZ_PTHREAD_INHERIT_SCHED   0
#define NZ_PTHREAD_EXPLICIT_SCHED  1

#define NZ_PTHREAD_SCOPE_SYSTEM    0
#define NZ_PTHREAD_SCOPE_PROCESS   1

#define NZ_PTHREAD_PROCESS_PRIVATE 	0
#define NZ_PTHREAD_PROCESS_SHARED	1

#define NZ_PTHREAD_MUTEX_DEFAULT       0
#define NZ_PTHREAD_MUTEX_NORMAL        1
#define NZ_PTHREAD_MUTEX_ERRORCHECK    2
#define NZ_PTHREAD_MUTEX_RECURSIVE     3
#define NZ_NUM_MUTEX_TYPE              4

typedef mutex_t nz_pthread_mutex_t;

struct nz_sched_param {
    int sched_priority;
};

typedef struct {
    uint8_t shared;
    uint8_t type;
} nz_pthread_mutexattr_t;

typedef struct {
    uint8_t policy;
    uint8_t inheritsched;
    uint8_t detachstate;
    uint8_t scope;

    uint32_t stacksize;
    uint32_t guardsize;
    void *stackaddr;
    struct nz_sched_param schedparam;
} nz_pthread_attr_t;

typedef struct {
	uint32_t id;
    void *(*start_routine)(void *);
    void *arg;
} nz_pthread_t;

// pthread primitives
int nz_pthread_create(nz_pthread_t *thread, const nz_pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
int nz_pthread_join(nz_pthread_t thread, void **retval);
void nz_pthread_exit(void *retval);
int nz_pthread_detach(nz_pthread_t thread);
int nz_pthread_yield(void);
nz_pthread_t nz_pthread_self(void);
int nz_pthread_kill(nz_pthread_t thread, int sig);
int nz_pthread_equal(nz_pthread_t t1, nz_pthread_t t2);

// attr
// initialize and destroy thread attributes object
int nz_pthread_attr_init(nz_pthread_attr_t *attr);
int nz_pthread_attr_destroy(nz_pthread_attr_t *attr);

// set and get the detach state attribute 
int nz_pthread_attr_setdetachstate(nz_pthread_attr_t *attr, int detachstate);
int nz_pthread_attr_getdetachstate(const nz_pthread_attr_t *attr, int *detachstate);

// set and get the stack size attribute 
int nz_pthread_attr_setstacksize(nz_pthread_attr_t *attr, size_t stacksize);
int nz_pthread_attr_getstacksize(const nz_pthread_attr_t *attr, size_t *stacksize);

// set and get the stack address attribute (deprecated in some systems)
int nz_pthread_attr_setstackaddr(nz_pthread_attr_t *attr, void *stackaddr);
int nz_pthread_attr_getstackaddr(const nz_pthread_attr_t *attr, void **stackaddr);

// set and get the stack attribute
int nz_pthread_attr_setstack(nz_pthread_attr_t *attr, void *stackaddr, size_t stacksize);
int nz_pthread_attr_getstack(const nz_pthread_attr_t *attr, void **stackaddr, size_t *stacksize);

// set and get the guard size attribute
int nz_pthread_attr_setguardsize(nz_pthread_attr_t *attr, size_t guardsize);
int nz_pthread_attr_getguardsize(const nz_pthread_attr_t *attr, size_t *guardsize);

// set and get the scheduling parameter attributes
int nz_pthread_attr_setschedparam(nz_pthread_attr_t *attr, const struct nz_sched_param *param);
int nz_pthread_attr_getschedparam(const nz_pthread_attr_t *attr, struct nz_sched_param *param);

// set and get the scheduling policy attribute 
int nz_pthread_attr_setschedpolicy(nz_pthread_attr_t *attr, int policy);
int nz_pthread_attr_getschedpolicy(const nz_pthread_attr_t *attr, int *policy);

// set and get the inherit scheduling attribute 
int nz_pthread_attr_setinheritsched(nz_pthread_attr_t *attr, int inheritsched);
int nz_pthread_attr_getinheritsched(const nz_pthread_attr_t *attr, int *inheritsched);

// set and get the scope attribute
int nz_pthread_attr_setscope(nz_pthread_attr_t *attr, int scope);
int nz_pthread_attr_getscope(const nz_pthread_attr_t *attr, int *scope);

// mutex
// mutex initialization
int nz_pthread_mutex_init(nz_pthread_mutex_t *mutex, const nz_pthread_mutexattr_t *attr);

// destroy the mutex
int nz_pthread_mutex_destroy(nz_pthread_mutex_t *mutex);

// lock the mutex
int nz_pthread_mutex_lock(nz_pthread_mutex_t *mutex);

// try to lock the mutex
int nz_pthread_mutex_trylock(nz_pthread_mutex_t *mutex);

// unlock the mutex
int nz_pthread_mutex_unlock(nz_pthread_mutex_t *mutex);

// initialize a mutex attributes object
int nz_pthread_mutexattr_init(nz_pthread_mutexattr_t *attr);

// destroy a mutex attributes object
int nz_pthread_mutexattr_destroy(nz_pthread_mutexattr_t *attr);

// set and get the process-shared attribute
int nz_pthread_mutexattr_getpshared(const nz_pthread_mutexattr_t *attr, int *pshared);
int nz_pthread_mutexattr_setpshared(nz_pthread_mutexattr_t *attr, int pshared);

// set and get the type attribute (e.g., PTHREAD_MUTEX_NORMAL)
int nz_pthread_mutexattr_gettype(const nz_pthread_mutexattr_t *attr, int *type);
int nz_pthread_mutexattr_settype(nz_pthread_mutexattr_t *attr, int type);

typedef cond_t nz_pthread_cond_t;

typedef struct {
    nz_clockid_t clock_id;
    uint8_t shared;
} nz_pthread_condattr_t;

int nz_pthread_cond_init(nz_pthread_cond_t *restrict cond, const nz_pthread_condattr_t *restrict attr);
int nz_pthread_cond_destroy(nz_pthread_cond_t *cond);
int nz_pthread_cond_wait(nz_pthread_cond_t *restrict cond, nz_pthread_mutex_t *restrict mutex);
int nz_pthread_cond_timedwait(nz_pthread_cond_t *restrict cond, nz_pthread_mutex_t *restrict mutex, const struct nz_timespec *restrict abstime);
int nz_pthread_cond_signal(nz_pthread_cond_t *cond);
int nz_pthread_cond_broadcast(nz_pthread_cond_t *cond);
int nz_pthread_condattr_init(nz_pthread_condattr_t *attr);
int nz_pthread_condattr_destroy(nz_pthread_condattr_t *attr);
int nz_pthread_condattr_setpshared(nz_pthread_condattr_t *attr, int pshared);
int nz_pthread_condattr_getpshared(const nz_pthread_condattr_t *restrict attr, int *restrict pshared);
int nz_pthread_condattr_setclock(nz_pthread_condattr_t *attr, nz_clockid_t clock_id);
int nz_pthread_condattr_getclock(const nz_pthread_condattr_t *restrict attr, nz_clockid_t *restrict clock_id);

typedef spinlock_t nz_pthread_spinlock_t;
// spinlock
int nz_pthread_spin_init(nz_pthread_spinlock_t *lock, int pshared);
int nz_pthread_spin_destroy(nz_pthread_spinlock_t *lock);
int nz_pthread_spin_lock(nz_pthread_spinlock_t *lock);
int nz_pthread_spin_trylock(nz_pthread_spinlock_t *lock);
int nz_pthread_spin_unlock(nz_pthread_spinlock_t *lock);

///////////////////////////////////////////////////////////
// not supported
int nz_pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
