#include "pthread.h"

int mutex_acquire(mutex_t *mutex);
int mutex_release(mutex_t *mutex);
int mutex_lock(mutex_t *mutex);
int mutex_trylock(mutex_t *mutex);
int mutex_unlock(mutex_t *mutex);

// mutex implementation
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    return mutex_acquire(mutex);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    return mutex_release(mutex);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return mutex_lock(mutex);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return mutex_trylock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return mutex_unlock(mutex);
}
