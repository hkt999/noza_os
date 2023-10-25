#include "pthread.h"
#include "sched.h"
#include "nozaos.h"
#include "errno.h"
#include "noza_time.h"

int nz_pthread_cond_init(nz_pthread_cond_t *restrict cond, const nz_pthread_condattr_t *restrict attr)
{
    if (cond == NULL)
        return EINVAL;

    nz_pthread_mutex_init(&cond->internal_mutex, NULL);
    cond->signaled = 0;
    cond->user_mutex = NULL;

    return 0;
}

int nz_pthread_cond_destroy(nz_pthread_cond_t *cond)
{
    if (cond == NULL)
        return EINVAL;
    return 0;
}

int nz_pthread_cond_wait(nz_pthread_cond_t *restrict cond, nz_pthread_mutex_t *restrict mutex)
{
    return nz_pthread_cond_timedwait(cond, mutex, NULL);
}

int nz_pthread_cond_timedwait(nz_pthread_cond_t *restrict cond, nz_pthread_mutex_t *restrict mutex, const struct nz_timespec *restrict abstime)
{
    int result = 0, error = 0;
    if (cond == NULL || mutex == NULL || abstime == NULL) {
        return EINVAL;
    }

    nz_pthread_mutex_lock(&cond->internal_mutex);
    cond->user_mutex = mutex;
    int64_t remaining = -1;
    if (abstime) {
        remaining = abstime->tv_sec * 1000000 + abstime->tv_nsec / 1000;
    }

    while (!cond->signaled) {
        cond->waiters++;
        nz_pthread_mutex_unlock(mutex); // release mutex
        nz_pthread_mutex_unlock(&cond->internal_mutex); // releae internal mutex for other threads to signal or wait

        struct nz_timespec d, r;
        if (remaining == -1) { // forever
            d.tv_sec = 10;
            d.tv_nsec = 0;
            nz_nanosleep(&d, &r); // sleep for 10 seconds
            nz_pthread_mutex_lock(&cond->internal_mutex);
            cond->waiters--;
            nz_pthread_mutex_lock(mutex);
        } else {
            d.tv_sec = remaining / 1000000;
            d.tv_nsec = (remaining % 1000000) * 1000;
            result = nz_nanosleep(&d, &r); // sleep for the remaining time
            remaining = r.tv_sec * 1000000 + r.tv_nsec / 1000;
            if (result == 0) {
                nz_pthread_mutex_lock(&cond->internal_mutex); // release internal mutex
                cond->waiters--; // update waiters count
                nz_pthread_mutex_lock(mutex); // lock user's mutex
                error = ETIMEDOUT;
                break;
            }
        }
    }

    if (cond->signaled) {
        nz_pthread_mutex_lock(mutex);
        cond->signaled = 0; // clear the signal for future waiters
    }

    cond->waiters--;
    nz_pthread_mutex_unlock(&cond->internal_mutex);
    return error;
}

int nz_pthread_cond_signal(nz_pthread_cond_t *cond)
{
    if (cond == NULL) {
        return EINVAL;
    }
    nz_pthread_mutex_lock(&cond->internal_mutex);
    cond->signaled = 1;
    nz_pthread_mutex_unlock(cond->user_mutex);   // wake up the waiting thread by unlocking user's mutex
    nz_pthread_mutex_unlock(&cond->internal_mutex);
    return 0;
}

// TODO: reconsider again !!
int nz_pthread_cond_broadcast(nz_pthread_cond_t *cond)
{
    #if 0
    if (cond == NULL) {
        return EINVAL;
    }
    pthread_mutex_lock(&cond->internal_mutex);
    cond->signaled = 1;
    pthread_mutex_unlock(cond->user_mutex);   // wake up the waiting thread by unlocking user's mutex

    while (cond->waiters > 0) {
        pthread_mutex_unlock(&cond->internal_mutex);
        sched_yield();  // give up the CPU to allow waiting threads to acquire the mutex
        pthread_mutex_lock(&cond->internal_mutex);
    }
    cond->signaled = 0;
    pthread_mutex_unlock(&cond->internal_mutex);
    #endif
    return 0;
}
