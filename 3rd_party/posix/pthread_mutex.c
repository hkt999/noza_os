#include "pthread.h"

/* mutex initialization */
int nz_pthread_mutex_init(nz_pthread_mutex_t *mutex, const nz_pthread_mutexattr_t *attr)
{
    return mutex_acquire(mutex);
}

/* destroy the mutex */
int nz_pthread_mutex_destroy(nz_pthread_mutex_t *mutex)
{
    return mutex_release(mutex);
}

/* lock the mutex */
int nz_pthread_mutex_lock(nz_pthread_mutex_t *mutex)
{
    return mutex_lock(mutex);
}

/* try to lock the mutex */
int nz_pthread_mutex_trylock(nz_pthread_mutex_t *mutex)
{
    return mutex_trylock(mutex);
}

/* unlock the mutex */
int nz_pthread_mutex_unlock(nz_pthread_mutex_t *mutex)
{
    return mutex_unlock(mutex);
}
