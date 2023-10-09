#include "semaphore.h"

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (pshared) {
        // this simplified implementation does not support shared semaphores
        return -1;
    }

    sem->count = value;
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    return 0;
}

int sem_destroy(sem_t *sem)
{
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
    return 0;
}

int sem_wait(sem_t *sem)
{
    pthread_mutex_lock(&sem->mutex);
    while (sem->count <= 0) {
        pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

int sem_post(sem_t *sem)
{
    pthread_mutex_lock(&sem->mutex);
    sem->count++;
    pthread_cond_signal(&sem->cond); // signal one waiting thread
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

int sem_trywait(sem_t *sem)
{
    int ret = 0;
    pthread_mutex_lock(&sem->mutex);
    if (sem->count > 0) {
        sem->count--;
    } else {
        ret = EAGAIN; // try again error, as per POSIX
    }
    pthread_mutex_unlock(&sem->mutex);
    return ret;
}

int sem_getvalue(sem_t *sem, int *sval)
{
    pthread_mutex_lock(&sem->mutex);
    *sval = sem->count;
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}