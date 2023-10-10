#include "pthread.h"

int pthread_condattr_init(pthread_condattr_t *attr)
{
    if (attr == NULL)
        return EINVAL;

    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr)
{
    if (attr == NULL)
        return EINVAL;

    return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared)
{
    attr->shared = pshared;
    return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t *restrict attr, int *restrict pshared)
{
    *pshared = attr->shared;
    return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id)
{
    attr->clock_id = clock_id;
    return 0;
}

int pthread_condattr_getclock(const pthread_condattr_t *restrict attr, clockid_t *restrict clock_id)
{
    *clock_id = attr->clock_id;
    return 0;
}