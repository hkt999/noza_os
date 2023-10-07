#include "pthread.h"
#include "nozaos.h"

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

static void noza_thread_stub(void *param, uint32_t pid)
{
    pthread_t *th = param;
    th->id = pid;
    th->start_routine(th->arg);
}

// pthread routines
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
    thread->start_routine = start_routine;
    thread->arg = arg;
    noza_thread_create(noza_thread_stub, thread, 0); // TODO: add attr support

    return 0;
}

int pthread_join(pthread_t thread, void **retval)
{
    if (retval != 0) {
        *retval = 0;
    }
    return noza_thread_join(thread.id); // TODO: handle the return code
}

int pthread_exit(void *retval)
{
    int exit_code = 0;
    if (retval) {
        exit_code = *((int *)retval);
    }
    noza_thread_terminate(exit_code);
    return 0;
}

int pthread_detach(pthread_t thread)
{
    return 0; // TODO: handle the detach logic
}

void pthread_yield(void)
{
    noza_thread_yield();
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1.id == t2.id;
}