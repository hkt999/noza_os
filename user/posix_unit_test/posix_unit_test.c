#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "kernel/noza_config.h"
#include "posix/bits/signum.h"
#include "posix/errno.h"
#include "posix/noza_time.h"
#include "posix/pthread.h"
#include "posix/sched.h"
#include "posix/semaphore.h"
#define UNITY_INCLUDE_CONFIG_H
#include "unity.h"
#include "posix/noza_posix_wrapper.h"

static void *test_task(void *param)
{
    int do_count = rand() % 5 + 2;
    int ms = rand() % 50 + 50;
    while (do_count-->0) {
        struct timespec req, rem;
        req.tv_sec = ms / 1000;
        req.tv_nsec = (ms % 1000) * 1000000;
        nanosleep(&req, &rem);
    }

    return (void *)0x123;
}

static void *yield_test_func(void *param)
{
    #define YIELD_ITER  50
    uint32_t value = 0;
    for (int i=0; i<YIELD_ITER; i++) {
        value = value + i;
        if (param == NULL) {
            TEST_ASSERT_EQUAL_INT(0, pthread_yield()); // test pthread_yield
            TEST_ASSERT_EQUAL_INT(0, sched_yield()); // test sched_yield
        }
    }

    return (void *)value;
}

static void *heavy_test_func(void *param)
{
    uint32_t a0 = 1, a1 = 1;
    int *flag = (int *)param;

    #define HEAVY_ITER  2000000
    uint32_t value = 0;
    for (int i=0; i<HEAVY_ITER; i++) {
        value = a0 + a1;
        a0 = a1;
        a1 = value;
    }

    if (flag) {
        *flag = 1;
    }

    return (void *)value;
}

#define NUM_THREADS 8
#define NUM_LOOP    4
static void test_pthread_create_join()
{
    pthread_t th[NUM_THREADS];
    srand(time(0));
    for (int loop = 0; loop < NUM_LOOP; loop++) {
        // TEST
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL_INT(0, pthread_create(&th[i], NULL, test_task, NULL));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL_INT(0, pthread_join(th[i], (void *)&exit_code));
            TEST_ASSERT_EQUAL_INT(0x123, exit_code);
        }
    }
}

static void test_pthread_kill()
{
    pthread_t th[NUM_THREADS];
    srand(time(0));
    for (int loop = 0; loop < NUM_LOOP; loop++) {
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL_INT(0, pthread_create(&th[i], NULL, test_task, NULL));
        }
        TEST_ASSERT_EQUAL_INT(0, nanosleep((&(struct timespec){.tv_sec=0, .tv_nsec=50000000}), NULL));
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t sig = 0;
            TEST_ASSERT_EQUAL_INT(0, pthread_kill(th[i], SIGALRM));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL_INT(0, pthread_join(th[i], (void **)&exit_code));
        }
    }
}

static void test_heavy_loading()
{
    pthread_t th[NUM_THREADS];
    uint32_t value_heavy = (uint32_t)heavy_test_func(NULL);
    for (int loop = 0; loop < NUM_LOOP; loop++) {
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL_INT(0, pthread_create(&th[i], NULL, heavy_test_func, NULL));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL_INT(0, pthread_join(th[i], (void **)&exit_code));
            TEST_ASSERT_EQUAL_UINT(value_heavy, exit_code);
        }
    }
}

static void test_pthread_yield()
{
    pthread_t th[NUM_THREADS];
    for (int loop = 0; loop < NUM_LOOP; loop++) {
        uint32_t value_yield = (uint32_t)yield_test_func((void *)1);
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL_INT(0, pthread_create(&th[i], NULL, yield_test_func, NULL));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL_INT(0, pthread_join(th[i], (void **)&exit_code));
            TEST_ASSERT_EQUAL_INT(value_yield, exit_code);
        }
    }
}

static void test_pthread_detach()
{
    pthread_t th[NUM_THREADS];
    int value[NUM_THREADS];

    for (int loop=0; loop < NUM_LOOP; loop++) {
        memset(value, 0, sizeof(value));
        for (int i=0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL_INT(0, pthread_create(&th[i], NULL, heavy_test_func, &value[i]));
            TEST_ASSERT_EQUAL_INT(0, pthread_detach(th[i]));
        }
        for (int i=0; i < NUM_THREADS; i++) {
            TEST_ASSERT_NOT_EQUAL(0, pthread_join(th[i], NULL));
            if (value[i] == 0) {
                TEST_ASSERT_EQUAL_INT(0, nz_nanosleep(&(struct timespec){.tv_sec=0, .tv_nsec=50000000}, NULL));
            }
        }
    }
}

#define ITERS 3000
static int counter = 0;
static void *inc_task(void *param)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        pthread_mutex_lock(mutex);
        counter++;
        pthread_mutex_unlock(mutex);
    }
}

static void *dec_task(void *param)
{
    mutex_t *mutex = (mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        pthread_mutex_lock(mutex);
        counter--;
        pthread_mutex_unlock(mutex);
    }
}

#define NUM_PAIR    4
static void test_pthread_mutex()
{
    pthread_mutex_t posix_mutex;
    // test posix mutex acquire/release function
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_init(&posix_mutex, NULL));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_destroy(&posix_mutex));

    // test mutex lock/unlock function
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_init(&posix_mutex, NULL));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_lock(&posix_mutex));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_unlock(&posix_mutex));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_trylock(&posix_mutex));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_unlock(&posix_mutex));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_destroy(&posix_mutex));

    // test posix mutex lock/unlock function with thread
    pthread_t inc_th[NUM_PAIR];
    pthread_t dec_th[NUM_PAIR];
    counter = 0; 
    pthread_mutex_init(&posix_mutex, NULL);
    for (int i = 0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL_INT(0, pthread_create(&inc_th[i], NULL, inc_task, &posix_mutex));
    }
    for (int i=0; i < NUM_PAIR; i++) {
        pthread_create(&dec_th[i], NULL, dec_task, &posix_mutex);
    }
    for (int i=0; i < NUM_PAIR; i++) {
        pthread_join(inc_th[i], NULL);
        pthread_join(dec_th[i], NULL);
    }
    TEST_ASSERT_EQUAL_INT(0, counter);

    // test posix mutex trylock
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_trylock(&posix_mutex));
    TEST_ASSERT_EQUAL_INT(EBUSY, pthread_mutex_trylock(&posix_mutex));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_unlock(&posix_mutex));
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_destroy(&posix_mutex));
}

void test_pthread_attr_init_and_destroy(void) {
    pthread_attr_t attr;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_destroy(&attr));
}

void test_pthread_attr_set_and_get_detachstate(void) {
    nz_pthread_attr_t attr;
    int detachstate = 1;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));

    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setdetachstate(&attr, detachstate));
    detachstate = 0; 
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getdetachstate(&attr, &detachstate));
    TEST_ASSERT_EQUAL_INT(1, detachstate);

    TEST_ASSERT_EQUAL_INT(0, pthread_attr_destroy(&attr));
}

void test_pthread_attr_set_and_get_stacksize(void) {
    nz_pthread_attr_t attr;
    size_t stacksize = 1024;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setstacksize(&attr, stacksize));
    stacksize = 0;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getstacksize(&attr, &stacksize));
    TEST_ASSERT_EQUAL_INT(1024, stacksize);

    TEST_ASSERT_EQUAL_INT(0, pthread_attr_destroy(&attr));
}

void test_pthread_attr_set_and_get_stackaddr(void) {
    pthread_attr_t attr;
    void* stackaddr = (void *)0x12345; 
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setstackaddr(&attr, stackaddr));
    stackaddr = 0;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getstackaddr(&attr, &stackaddr));
    TEST_ASSERT_EQUAL_INT(0x12345, stackaddr);

    TEST_ASSERT_EQUAL_INT(0, pthread_attr_destroy(&attr));
}

void test_pthread_attr_set_and_get_stack(void) {
    pthread_attr_t attr;
    void* stackaddr = (void *)0x12345; 
    size_t stacksize = 1024;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setstack(&attr, stackaddr, stacksize));
    stackaddr = 0;
    stacksize = 0;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getstack(&attr, &stackaddr, &stacksize));
    TEST_ASSERT_EQUAL_INT(0x12345, stackaddr);
    TEST_ASSERT_EQUAL_INT(1024, stacksize);

    TEST_ASSERT_EQUAL_INT(0, pthread_attr_destroy(&attr));
}

void test_pthread_attr_set_and_get_guardsize(void) {
    pthread_attr_t attr;
    size_t guardsize = 1024;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setguardsize(&attr, guardsize));
    guardsize = 0;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getguardsize(&attr, &guardsize));
    TEST_ASSERT_EQUAL_INT(1024, guardsize);

    TEST_ASSERT_EQUAL_INT(0, pthread_attr_destroy(&attr));
}

void test_pthread_attr_set_and_get_schedparam(void) {
    pthread_attr_t attr;
    struct sched_param param;
    param.sched_priority = 1;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setschedparam(&attr, &param));
    param.sched_priority = 0;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getschedparam(&attr, &param));
    TEST_ASSERT_EQUAL_INT(1, param.sched_priority);
}

void test_pthread_attr_set_and_get_schedpolicy(void) {
    pthread_attr_t attr;
    int policy = SCHED_RR; // only support this
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setschedpolicy(&attr, policy));
    policy = 0;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getschedpolicy(&attr, &policy));
    TEST_ASSERT_EQUAL_INT(SCHED_RR, policy);
}

void test_pthread_attr_set_and_get_inheristsched(void) {
    pthread_attr_t attr;
    int inheritsched = 1;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setinheritsched(&attr, inheritsched));
    inheritsched = 0;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getinheritsched(&attr, &inheritsched));
    TEST_ASSERT_EQUAL_INT(1, inheritsched);
}

void test_pthread_attr_set_and_get_scope(void) {
    pthread_attr_t attr;
    int scope = PTHREAD_SCOPE_SYSTEM; // only support this
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_init(&attr));
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_setscope(&attr, scope));
    scope = 0;
    TEST_ASSERT_EQUAL_INT(0, pthread_attr_getscope(&attr, &scope));
    TEST_ASSERT_EQUAL_INT(PTHREAD_SCOPE_SYSTEM, scope);
}

static sem_t semaphore;
void* incrementer(void* arg) {
    for (int i = 0; i < 1000; ++i) {
        sem_wait(&semaphore);
        counter++;
        sem_post(&semaphore);
    }
    return NULL;
}

void* decrementer(void* arg) {
    for (int i = 0; i < 1000; ++i) {
        sem_wait(&semaphore);
        counter--;
        sem_post(&semaphore);
    }
    return NULL;
}

void test_semaphore_synchronization(void) {
    counter = 0;
    TEST_ASSERT_EQUAL_INT(0, sem_init(&semaphore, 0, 1));  // Initialize semaphore with value 1.

    pthread_t inc_thread, dec_thread;

    TEST_ASSERT_EQUAL_INT(0, pthread_create(&inc_thread, NULL, incrementer, NULL));
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&dec_thread, NULL, decrementer, NULL));

    TEST_ASSERT_EQUAL_INT(0, pthread_join(inc_thread, NULL));
    TEST_ASSERT_EQUAL_INT(0, pthread_join(dec_thread, NULL));

    TEST_ASSERT_EQUAL_INT(0, counter);  // After both threads finish, counter should be 0.
    TEST_ASSERT_EQUAL_INT(0, sem_destroy(&semaphore));  // Clean up.
}

static int test_posix(int argc, char **argv)
{
    UNITY_BEGIN();
    TEST_MESSAGE("test posix unit-test suite start, please wait...");
    RUN_TEST(test_pthread_create_join);
    RUN_TEST(test_pthread_yield);
    RUN_TEST(test_pthread_detach);
    RUN_TEST(test_pthread_kill);
    RUN_TEST(test_pthread_mutex);
    RUN_TEST(test_heavy_loading);
    //RUN_TEST(test_semaphore_synchronization);
    RUN_TEST(test_pthread_attr_init_and_destroy);
    RUN_TEST(test_pthread_attr_set_and_get_detachstate);
    RUN_TEST(test_pthread_attr_set_and_get_stacksize);
    RUN_TEST(test_pthread_attr_set_and_get_stackaddr);
    RUN_TEST(test_pthread_attr_set_and_get_stack);
    RUN_TEST(test_pthread_attr_set_and_get_guardsize);
    RUN_TEST(test_pthread_attr_set_and_get_schedparam);
    RUN_TEST(test_pthread_attr_set_and_get_schedpolicy);
    RUN_TEST(test_pthread_attr_set_and_get_inheristsched);
    RUN_TEST(test_pthread_attr_set_and_get_scope);
    UNITY_END();
    return 0;
}

#include "user/console/noza_console.h"
void __attribute__((constructor(1000))) register_posix_unittest()
{
    console_add_command("posix_unittest", test_posix, "nozaos and lib, posix unit-test suite");
}

