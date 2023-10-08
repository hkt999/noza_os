#include "pthread.h"
#include <string.h>

// initialize and destroy thread attributes object
int pthread_attr_init(pthread_attr_t *attr)
{
    memset(attr, 0, sizeof(pthread_attr_t));
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->stacksize = 0x1000;
    attr->guardsize = 0x1000 - 16;
    attr->schedparam.sched_priority = 0; // TODO: consider the range
    attr->policy = SCHED_RR;
    attr->inheritsched = PTHREAD_EXPLICIT_SCHED;
    attr->scope = PTHREAD_SCOPE_SYSTEM;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    // do nothing
    return 0;
}

// set and get the detach state attribute 
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
    attr->detachstate = detachstate;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
    *detachstate = attr->detachstate;
    return 0;
}

// set and get the stack size attribute 
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    *stacksize = attr->stacksize;
    return 0;
}

// set and get the stack address attribute (deprecated in some systems)
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
    attr->stackaddr = stackaddr;
    return 0;
}

int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
    *stackaddr = attr->stackaddr;
    return 0;
}

// set and get the stack attribute
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize)
{
    attr->stackaddr = stackaddr;
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize)
{
    *stackaddr = attr->stackaddr;
    *stacksize = attr->stacksize;
    return 0;
}

// set and get the guard size attribute
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
    attr->guardsize = guardsize;
    return 0;
}

int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
    *guardsize = attr->guardsize;
    return 0;
}

// set and get the scheduling parameter attributes
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param)
{
    attr->schedparam.sched_priority = param->sched_priority;
    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param)
{
    param->sched_priority = attr->schedparam.sched_priority;
    return 0;
}

// set and get the scheduling policy attribute 
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
    if (policy != SCHED_RR) {
        return ENOTSUP;
    }
    attr->policy = policy;
    return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
    *policy = attr->policy;
    return 0;
}

// set and get the inherit scheduling attribute 
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched)
{
    attr->inheritsched = inheritsched;
    return 0;
}

int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched)
{
    *inheritsched = attr->inheritsched;
    return 0;
}

// set and get the scope attribute
int pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
    if (scope != PTHREAD_SCOPE_SYSTEM) {
        return ENOTSUP;
    }
    attr->scope = scope;
    return 0;
}

int pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
    *scope = attr->scope;
    return 0;
}