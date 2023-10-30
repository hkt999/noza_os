#include "pthread.h"

int nz_pthread_condattr_init(nz_pthread_condattr_t *attr)
{
    if (attr == NULL)
        return EINVAL;

    return 0;
}

int nz_pthread_condattr_destroy(nz_pthread_condattr_t *attr)
{
    if (attr == NULL)
        return EINVAL;

    return 0;
}

int nz_pthread_condattr_setpshared(nz_pthread_condattr_t *attr, int pshared)
{
    attr->shared = pshared;
    return 0;
}

int nz_pthread_condattr_getpshared(const nz_pthread_condattr_t *restrict attr, int *restrict pshared)
{
    *pshared = attr->shared;
    return 0;
}

int nz_pthread_condattr_setclock(nz_pthread_condattr_t *attr, nz_clockid_t clock_id)
{
    attr->clock_id = clock_id;
    return 0;
}

int nz_pthread_condattr_getclock(const nz_pthread_condattr_t *restrict attr, nz_clockid_t *restrict clock_id)
{
    *clock_id = attr->clock_id;
    return 0;
}