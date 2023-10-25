#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "kernel/noza_config.h"
#include "posix/bits/signum.h"
#include "posix/errno.h"
#include "posix/noza_time.h"
#include "posix/pthread.h"
#define UNITY_INCLUDE_CONFIG_H
#include "unity.h"

static void *test_task(void *param)
{
    int do_count = rand() % 5 + 2;
    int ms = rand() % 50 + 50;
    while (do_count-->0) {
        struct nz_timespec req, rem;
        req.tv_sec = ms / 1000;
        req.tv_nsec = (ms % 1000) * 1000000;
        TEST_ASSERT_EQUAL(0, nz_nanosleep(&req, &rem));
    }
}

static void *yield_test_func(void *param)
{
    #define YIELD_ITER  100
    uint32_t value = 0;
    for (int i=0; i<YIELD_ITER; i++) {
        value = value + i;
        if (param == NULL) {
            TEST_ASSERT_EQUAL(0, nz_pthread_yield());
        }
    }

    return (void *)value;
}

static void *heavy_test_func(void *param)
{
    int *flag = (int *)param;
    #define HEAVY_ITER  1000000
    uint32_t value = 0;
    for (int i=0; i<HEAVY_ITER; i++) {
        value = value + i;
    }

    if (flag) {
        *flag = 1;
    }

    return (void *)value;
}

static void do_test_thread()
{
    #define NUM_THREADS 8
    #define NUM_LOOP    16
    nz_pthread_t th[NUM_THREADS];
    nz_pthread_t pid = nz_pthread_self();

    for (int loop = 0; loop < NUM_LOOP; loop++) {
        TEST_PRINTF("test loop: %d/%d", loop, NUM_LOOP);
        srand(time(0));

        // TEST
        TEST_MESSAGE("---- test thread creation with random priority and sleep time ----");
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, nz_pthread_create(&th[i], NULL, test_task, NULL));
        }

        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, nz_pthread_join(th[i], (void **)&exit_code));
        }

        // TEST
        TEST_MESSAGE("---- test thread creation with random priority and signal in 100ms ----");
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, nz_pthread_create(&th[i], NULL, test_task, NULL));
        }
        nz_nanosleep(&(struct nz_timespec){.tv_sec=0, .tv_nsec=50000000}, NULL);
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t sig = 0;
            TEST_ASSERT_EQUAL(0, nz_pthread_kill(th[i], SIGALRM));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, nz_pthread_join(th[i], (void **)&exit_code));
        }

        // TEST
        TEST_MESSAGE("---- test heavy loading ----");
        uint32_t value_heavy = (uint32_t)heavy_test_func(NULL);
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, nz_pthread_create(&th[i], NULL, heavy_test_func, NULL));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, nz_pthread_join(th[i], (void **)&exit_code));
            TEST_ASSERT_EQUAL_UINT(value_heavy, exit_code);
        }

        // TEST
        TEST_MESSAGE("---- test yield ----");
        uint32_t value_yield = (uint32_t)yield_test_func(NULL);
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, nz_pthread_create(&th[i], NULL, yield_test_func, NULL));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, nz_pthread_join(th[i], (void **)&exit_code));
            TEST_ASSERT_EQUAL(value_yield, exit_code);
        }

        // TEST
        TEST_MESSAGE("---- test detach ----");
        int value[NUM_THREADS];
        memset(value, 0, sizeof(value));
        for (int i=0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, nz_pthread_create(&th[i], NULL, heavy_test_func, &value[i]));
            TEST_ASSERT_EQUAL(0, nz_pthread_detach(th[i]));
        }
        for (int i=0; i < NUM_THREADS; i++) {
            TEST_ASSERT_NOT_EQUAL(0, nz_pthread_join(th[i], NULL));
            if (value[i] == 0) {
                TEST_ASSERT_EQUAL(0, nz_nanosleep(&(struct nz_timespec){.tv_sec=0, .tv_nsec=50000000}, NULL));
            }
        }
    }
}

#define ITERS 3000
static int counter = 0;
static void *inc_task(void *param)
{
    nz_pthread_mutex_t *mutex = (nz_pthread_mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        nz_pthread_mutex_lock(mutex);
        counter++;
        nz_pthread_mutex_unlock(mutex);
    }
}

static void *dec_task(void *param)
{
    mutex_t *mutex = (mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        nz_pthread_mutex_lock(mutex);
        counter--;
        nz_pthread_mutex_unlock(mutex);
    }
}

#define NUM_PAIR    4
static void do_test_mutex()
{
    mutex_t noza_mutex;
    TEST_MESSAGE("test mutex acquire/release function");
    TEST_ASSERT_EQUAL(0, mutex_acquire(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_release(&noza_mutex));

    TEST_MESSAGE("test mutex lock/unlock function");
    TEST_ASSERT_EQUAL(0, mutex_acquire(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_lock(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_unlock(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_unlock(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_release(&noza_mutex));

    TEST_MESSAGE("test mutex lock/unlock function with thread");
    nz_pthread_t inc_th[NUM_PAIR];
    nz_pthread_t dec_th[NUM_PAIR];
    counter = 0; 
    mutex_acquire(&noza_mutex);
    for (int i = 0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL(0, nz_pthread_create(&inc_th[i], NULL, inc_task, &noza_mutex));
    }
    for (int i=0; i < NUM_PAIR; i++) {
        nz_pthread_create(&dec_th[i], NULL, dec_task, &noza_mutex);
    }
    for (int i=0; i < NUM_PAIR; i++) {
        nz_pthread_join(inc_th[i], NULL);
        nz_pthread_join(dec_th[i], NULL);
    }
    TEST_ASSERT_EQUAL_INT(0, counter);
    TEST_MESSAGE("test mutex trylock");
    TEST_ASSERT_EQUAL(0, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL(EBUSY, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_unlock(&noza_mutex));
    mutex_release(&noza_mutex);
}

static int test_mutex(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_mutex);
    UNITY_END();
    return 0;
}

static int test_all(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_thread);
    RUN_TEST(do_test_mutex);
    UNITY_END();
    return 0;
}

#include "user/console/noza_console.h"
void __attribute__((constructor(1000))) register_posix_unittest()
{
    console_add_command("noza_unittest", test_all, "nozaos and lib, posix unit-test suite");
}

