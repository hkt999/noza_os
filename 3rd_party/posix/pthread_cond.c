#include "pthread.h"
#include "sched.h"
#include "nozaos.h"
#include "errno.h"
#include "noza_time.h"

int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr)
{
    if (cond == NULL)
        return EINVAL;

    pthread_mutex_init(&cond->internal_mutex, NULL);
    cond->signaled = 0;
    cond->user_mutex = NULL;

    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
    if (cond == NULL)
        return EINVAL;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex)
{
    return pthread_cond_timedwait(cond, mutex, NULL);
}

int pthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime)
{
    int result = 0, error = 0;
    if (cond == NULL || mutex == NULL || abstime == NULL) {
        return EINVAL;
    }

    pthread_mutex_lock(&cond->internal_mutex);
    cond->user_mutex = mutex;
    int64_t remaining = -1;
    if (abstime) {
        remaining = abstime->tv_sec * 1000000 + abstime->tv_nsec / 1000;
    }

    while (!cond->signaled) {
        cond->waiters++;
        pthread_mutex_unlock(mutex); // release mutex
        pthread_mutex_unlock(&cond->internal_mutex); // releae internal mutex for other threads to signal or wait

        struct timespec d, r;
        if (remaining == -1) { // forever
            d.tv_sec = 10;
            d.tv_nsec = 0;
            noza_nanosleep(&d, &r); // sleep for 10 seconds
            pthread_mutex_lock(&cond->internal_mutex);
            cond->waiters--;
            pthread_mutex_lock(mutex);
        } else {
            d.tv_sec = remaining / 1000000;
            d.tv_nsec = (remaining % 1000000) * 1000;
            result = noza_nanosleep(&d, &r); // sleep for the remaining time
            remaining = r.tv_sec * 1000000 + r.tv_nsec / 1000;
            if (result == 0) {
                pthread_mutex_lock(&cond->internal_mutex); // release internal mutex
                cond->waiters--; // update waiters count
                pthread_mutex_lock(mutex); // lock user's mutex
                error = ETIMEDOUT;
                break;
            }
        }
    }

    if (cond->signaled) {
        pthread_mutex_lock(mutex);
        cond->signaled = 0; // clear the signal for future waiters
    }

    cond->waiters--;
    pthread_mutex_unlock(&cond->internal_mutex);
    return error;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
    if (cond == NULL) {
        return EINVAL;
    }
    pthread_mutex_lock(&cond->internal_mutex);
    cond->signaled = 1;
    pthread_mutex_unlock(cond->user_mutex);   // wake up the waiting thread by unlocking user's mutex
    pthread_mutex_unlock(&cond->internal_mutex);
    return 0;
}

// TODO: reconsider again !!
int pthread_cond_broadcast(pthread_cond_t *cond)
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
