#include "pthread.h"
#include "errno.h"

/* initialize a mutex attributes object */
int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    attr->type = PTHREAD_MUTEX_DEFAULT;
    return -1;
}

/* destroy a mutex attributes object */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    // nothing to do
    return 0;
}

/* get the process-shared attribute */
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared)
{
    *pshared = attr->shared;
    return 0;
}

/* set the process-shared attribute */
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
    attr->shared = pshared;
    return 0;
}

/* get the type attribute (e.g., PTHREAD_MUTEX_NORMAL) */
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
    *type = attr->type;
    return 0;
}

/* set the type attribute */
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    attr->type = type;
    return 0;
}