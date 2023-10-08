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

/* Mutex initialization */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);

/* Destroy the mutex */
int pthread_mutex_destroy(pthread_mutex_t *mutex);

/* Lock the mutex */
int pthread_mutex_lock(pthread_mutex_t *mutex);

/* Try to lock the mutex */
int pthread_mutex_trylock(pthread_mutex_t *mutex);

/* Unlock the mutex */
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* Initialize a mutex attributes object */
int pthread_mutexattr_init(pthread_mutexattr_t *attr);

/* Destroy a mutex attributes object */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);

/* Get the process-shared attribute */
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared);

/* Set the process-shared attribute */
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);

/* Get the type attribute (e.g., PTHREAD_MUTEX_NORMAL) */
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);

/* Set the type attribute */
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);
