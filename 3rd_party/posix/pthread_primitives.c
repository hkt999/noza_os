#include "pthread.h"
#include "nozaos.h"

static int noza_thread_stub(void *param, uint32_t pid)
{
    pthread_t *th = param;
    th->id = pid;
    return (int) th->start_routine(th->arg);
}

#include "noza_config.h"
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

    uint32_t pid, priority = NOZA_OS_PRIORITY_LIMIT - 1 - wa->schedparam.sched_priority;
    uint32_t ret_code;

    if (wa->stackaddr != NULL) {
        ret_code = noza_thread_create_with_stack(&pid, noza_thread_stub, thread, priority, wa->stackaddr, wa->stacksize, NO_AUTO_FREE_STACK);
    } else {
        ret_code = noza_thread_create(&pid, noza_thread_stub, thread, priority, wa->stacksize);
    }
    if (ret_code != 0) {
        return ret_code;
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

void pthread_exit(void *retval)
{
    extern void noza_thread_exit(uint32_t exit_code);
    uint32_t exit_code = (uint32_t)retval;
    noza_thread_exit(exit_code);
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