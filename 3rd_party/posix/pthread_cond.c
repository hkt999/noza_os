#include "pthread.h"
#include "sched.h"
#include "nozaos.h"
#include "errno.h"
#include "noza_time.h"

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

int __pthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime)
{
    pthread_mutex_lock(&cond->internal_mutex);
    cond->user_mutex = mutex;

    while (!cond->signaled) {
        pthread_mutex_unlock(mutex);                    // release user's mutex
        pthread_mutex_unlock(&cond->internal_mutex);    // release internal mutex to allow signaling

        // check timeout here
        sched_yield(); // give up the processor for efficiency. This is a form of "busy-waiting".

        pthread_mutex_lock(&cond->internal_mutex);      // re-acquire internal mutex
        if (cond->signaled) {
            pthread_mutex_lock(mutex);                  // re-acquire user's mutex before returning
            cond->signaled = 0;                         // reset for potential future waits
        }
    }

    pthread_mutex_unlock(&cond->internal_mutex);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime)
{
    if (cond == NULL || mutex == NULL || abstime == NULL) {
        return EINVAL;
    }
    pthread_mutex_lock(&cond->internal_mutex);
    cond->user_mutex = mutex;
    cond->waiters++;
    int result = 0;
    while (!cond->signaled && result != ETIMEDOUT) {
        struct timespec now;
        noza_clock_gettime(CLOCK_REALTIME, &now);

        if (now.tv_sec > abstime->tv_sec || (now.tv_sec == abstime->tv_sec && now.tv_nsec >= abstime->tv_nsec)) {
            result = ETIMEDOUT;
            break;
        }
        struct timespec remaining;
        remaining.tv_sec = abstime->tv_sec - now.tv_sec;
        remaining.tv_nsec = abstime->tv_nsec - now.tv_nsec;
        if (remaining.tv_nsec < 0) {
            remaining.tv_sec--;
            remaining.tv_nsec += 1000000000;
        }
        pthread_mutex_unlock(mutex);
        pthread_mutex_unlock(&cond->internal_mutex);

        struct timespec sleep_time;
        sleep_time.tv_sec = remaining.tv_nsec / 1000000000;
        sleep_time.tv_nsec = remaining.tv_nsec % 1000000000;
        result = noza_nanosleep(&sleep_time, &remaining);

        pthread_mutex_lock(&cond->internal_mutex);
        pthread_mutex_lock(mutex);
    }

    if (cond->signaled) {
        pthread_mutex_lock(mutex);
        cond->signaled = 0;
    }

    cond->waiters--;
    pthread_mutex_unlock(&cond->internal_mutex);
    return result;
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
    return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared)
{
    return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t *restrict attr, int *restrict pshared)
{
    return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id)
{
    return 0;
}

int pthread_condattr_getclock(const pthread_condattr_t *restrict attr, clockid_t *restrict clock_id)
{
    return 0;
}