#include "pthread.h"

/* mutex initialization */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    return mutex_acquire(mutex);
}

/* destroy the mutex */
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    return mutex_release(mutex);
}

/* lock the mutex */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return mutex_lock(mutex);
}

/* try to lock the mutex */
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return mutex_trylock(mutex);
}

/* unlock the mutex */
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return mutex_unlock(mutex);
}
