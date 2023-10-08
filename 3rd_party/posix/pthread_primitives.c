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
    pthread_attr_t default_attr;
    pthread_attr_t *wa = (pthread_attr_t *)attr;
    if (wa == NULL) {
        pthread_attr_init(&default_attr);
        wa = &default_attr;
    }
    thread->start_routine = start_routine;
    thread->arg = arg;

    uint32_t pid;
    if (wa->stackaddr != NULL) {
        pid = noza_thread_create_with_stack(noza_thread_stub, thread, wa->schedparam.sched_priority, wa->stackaddr, wa->stacksize, NO_AUTO_FREE_STACK);
    } else {
        pid = noza_thread_create(noza_thread_stub, thread, wa->schedparam.sched_priority, wa->stacksize);
    }
    if (wa->detachstate == PTHREAD_CREATE_DETACHED) {
        noza_thread_detach(pid);
    }
    if (attr == NULL)
        pthread_attr_destroy(&default_attr);

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
    noza_thread_terminate(exit_code); // TODO: fix this with setjmp/longjump, or some method to free stack
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