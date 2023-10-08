#include "pthread.h"
#include "nozaos.h"

static int noza_thread_stub(void *param, uint32_t pid)
{
    pthread_t *th = param;
    th->id = pid;
    th->start_routine(th->arg);
    return 0; // TODO: handle the return code
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
    uint32_t code;
    int ret;

    ret = noza_thread_join(thread.id, &code); // TODO: handle the return code
    if (retval != 0) {
        *retval = (void *)code;
    }
    return ret;
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
    return noza_thread_detach(thread.id);
}

int pthread_yield(void)
{
    return noza_thread_yield();
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return t1.id == t2.id;
}