#include "pthread.h"
#include "errno.h"

// initialize a mutex attributes object
int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    attr->shared = PTHREAD_PROCESS_PRIVATE;
    attr->type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

// destroy a mutex attributes object
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    // do nothing
    return 0 ;
}

// get the process-shared attribute
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
    *pshared = attr->shared;
    return 0;
}

// set the process-shared attribute
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED)
        return EINVAL;

    attr->shared = pshared;
    return 0;
}

// get the type attribute (e.g., PTHREAD_MUTEX_NORMAL)
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
    *type = attr->type;
    return 0;
}

// set the type attribute
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    if (type < 0 || type >= NUM_MUTEX_TYPE)
        return EINVAL;

    attr->type = type;
    return 0;
}