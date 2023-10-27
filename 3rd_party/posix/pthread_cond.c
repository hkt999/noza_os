#include "pthread.h"
#include "sched.h"
#include "nozaos.h"
#include "errno.h"
#include "noza_time.h"
#include <string.h>
#include <stdio.h>

int nz_pthread_cond_init(nz_pthread_cond_t *restrict cond, const nz_pthread_condattr_t *restrict attr)
{
    return cond_acquire(cond);
}

int nz_pthread_cond_destroy(nz_pthread_cond_t *cond)
{
    return cond_release(cond);
}

int nz_pthread_cond_wait(nz_pthread_cond_t *restrict cond, nz_pthread_mutex_t *restrict mutex)
{
    return cond_wait(cond, mutex);
}

int nz_pthread_cond_timedwait(nz_pthread_cond_t *restrict cond, nz_pthread_mutex_t *restrict mutex, const struct nz_timespec *restrict abstime)
{
    return cond_timedwait(cond, mutex, abstime->tv_sec * 1000000 + abstime->tv_nsec / 1000);
}

int nz_pthread_cond_signal(nz_pthread_cond_t *cond)
{
    return cond_signal(cond);
}

int nz_pthread_cond_broadcast(nz_pthread_cond_t *cond)
{
    return cond_broadcast(cond);
}
