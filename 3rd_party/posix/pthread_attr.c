#include "pthread.h"
#include <string.h>

// initialize and destroy thread attributes object
int nz_pthread_attr_init(nz_pthread_attr_t *attr)
{
    memset(attr, 0, sizeof(nz_pthread_attr_t));
    attr->detachstate = NZ_PTHREAD_CREATE_JOINABLE;
    attr->stacksize = 2000; // TODO: reconsider this
    attr->guardsize = 0;
    attr->schedparam.sched_priority = 0;
    attr->policy = NZ_SCHED_RR;
    attr->inheritsched = NZ_PTHREAD_EXPLICIT_SCHED;
    attr->scope = NZ_PTHREAD_SCOPE_SYSTEM;
    return 0;
}

int nz_pthread_attr_destroy(nz_pthread_attr_t *attr)
{
    // do nothing
    return 0;
}

// set and get the detach state attribute 
int nz_pthread_attr_setdetachstate(nz_pthread_attr_t *attr, int detachstate)
{
    attr->detachstate = detachstate;
    return 0;
}

int nz_pthread_attr_getdetachstate(const nz_pthread_attr_t *attr, int *detachstate)
{
    *detachstate = attr->detachstate;
    return 0;
}

// set and get the stack size attribute 
int nz_pthread_attr_setstacksize(nz_pthread_attr_t *attr, size_t stacksize)
{
    attr->stacksize = stacksize;
    return 0;
}

int nz_pthread_attr_getstacksize(const nz_pthread_attr_t *attr, size_t *stacksize)
{
    *stacksize = attr->stacksize;
    return 0;
}

// set and get the stack address attribute (deprecated in some systems)
int nz_pthread_attr_setstackaddr(nz_pthread_attr_t *attr, void *stackaddr)
{
    attr->stackaddr = stackaddr;
    return 0;
}

int nz_pthread_attr_getstackaddr(const nz_pthread_attr_t *attr, void **stackaddr)
{
    *stackaddr = attr->stackaddr;
    return 0;
}

// set and get the stack attribute
int nz_pthread_attr_setstack(nz_pthread_attr_t *attr, void *stackaddr, size_t stacksize)
{
    attr->stackaddr = stackaddr;
    attr->stacksize = stacksize;
    return 0;
}

int nz_pthread_attr_getstack(const nz_pthread_attr_t *attr, void **stackaddr, size_t *stacksize)
{
    *stackaddr = attr->stackaddr;
    *stacksize = attr->stacksize;
    return 0;
}

// set and get the guard size attribute
int nz_pthread_attr_setguardsize(nz_pthread_attr_t *attr, size_t guardsize)
{
    attr->guardsize = guardsize;
    return 0;
}

int nz_pthread_attr_getguardsize(const nz_pthread_attr_t *attr, size_t *guardsize)
{
    *guardsize = attr->guardsize;
    return 0;
}

// set and get the scheduling parameter attributes
int nz_pthread_attr_setschedparam(nz_pthread_attr_t *attr, const struct nz_sched_param *param)
{
    attr->schedparam.sched_priority = param->sched_priority;
    return 0;
}

int nz_pthread_attr_getschedparam(const nz_pthread_attr_t *attr, struct nz_sched_param *param)
{
    param->sched_priority = attr->schedparam.sched_priority;
    return 0;
}

// set and get the scheduling policy attribute 
int nz_pthread_attr_setschedpolicy(nz_pthread_attr_t *attr, int policy)
{
    if (policy != NZ_SCHED_RR) {
        return ENOTSUP;
    }
    attr->policy = policy;
    return 0;
}

int nz_pthread_attr_getschedpolicy(const nz_pthread_attr_t *attr, int *policy)
{
    *policy = attr->policy;
    return 0;
}

// set and get the inherit scheduling attribute 
int nz_pthread_attr_setinheritsched(nz_pthread_attr_t *attr, int inheritsched)
{
    attr->inheritsched = inheritsched;
    return 0;
}

int nz_pthread_attr_getinheritsched(const nz_pthread_attr_t *attr, int *inheritsched)
{
    *inheritsched = attr->inheritsched;
    return 0;
}

// set and get the scope attribute
int nz_pthread_attr_setscope(nz_pthread_attr_t *attr, int scope)
{
    if (scope != NZ_PTHREAD_SCOPE_SYSTEM) {
        return ENOTSUP;
    }
    attr->scope = scope;
    return 0;
}

int nz_pthread_attr_getscope(const nz_pthread_attr_t *attr, int *scope)
{
    *scope = attr->scope;
    return 0;
}