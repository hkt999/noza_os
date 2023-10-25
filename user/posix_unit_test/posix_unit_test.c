#if 0

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "kernel/noza_config.h"
#include "nozaos.h"
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
        struct timespec req, rem;
        req.tv_sec = ms / 1000;
        req.tv_nsec = (ms % 1000) * 1000000;
        TEST_ASSERT_EQUAL(0, noza_nanosleep(&req, &rem));
    }
}

static void *yield_test_func(void *param)
{
    #define YIELD_ITER  100
    uint32_t value = 0;
    for (int i=0; i<YIELD_ITER; i++) {
        value = value + i;
        if (param == NULL) {
            TEST_ASSERT_EQUAL(0, pthread_yield());
        }
    }
}

static int heavy_test_func(void *param)
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
}

static void do_test_thread()
{
    #define NUM_THREADS 8
    #define NUM_LOOP    16
    thread_t th[NUM_THREADS];
    thread_t pid = pthread_self();

    for (int loop = 0; loop < NUM_LOOP; loop++) {
        TEST_PRINTF("test loop: %d/%d", loop, NUM_LOOP);
        srand(time(0));

        // TEST
        TEST_MESSAGE("---- test thread creation with random priority and sleep time ----");
        for (int i = 0; i < NUM_THREADS; i++) {
            //TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], test_task, NULL, (uint32_t)i%NOZA_OS_PRIORITY_LIMIT, 1024));
            TEST_ASSERT_TRUE(th[i] < NOZA_OS_TASK_LIMIT);
            TEST_ASSERT_NOT_EQUAL(0, th[i]);
        }

        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
            TEST_ASSERT_EQUAL(th[i], exit_code);
        }

        // TEST
        TEST_MESSAGE("---- test thread creation with random priority and signal in 100ms ----");
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], test_task, NULL, (uint32_t)i%NOZA_OS_PRIORITY_LIMIT, 1024));
            TEST_ASSERT_TRUE(th[i] < NOZA_OS_TASK_LIMIT);
            TEST_ASSERT_NOT_EQUAL(0, th[i]);
        }
        noza_thread_sleep_ms(50, NULL);
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t sig = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_kill(th[i], SIGALRM));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
            TEST_ASSERT_EQUAL(th[i], exit_code);
        }

        // TEST
        TEST_MESSAGE("---- test heavy loading ----");
        uint32_t value_heavy = heavy_test_func(NULL, pid);
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], heavy_test_func, NULL, 1, 1024));
            TEST_ASSERT_INT_WITHIN(NOZA_OS_TASK_LIMIT-1, NOZA_OS_TASK_LIMIT/2, th[i]);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
            TEST_ASSERT_EQUAL_UINT(value_heavy, exit_code);
        }

        // TEST
        TEST_MESSAGE("---- test yield ----");
        uint32_t value_yield = yield_test_func(&pid);
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], yield_test_func, NULL, 1, 1024));
            TEST_ASSERT_INT_WITHIN(NOZA_OS_TASK_LIMIT-1, NOZA_OS_TASK_LIMIT/2, th[i]);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
            TEST_ASSERT_EQUAL(value_yield, exit_code);
        }

        // TEST
        TEST_MESSAGE("---- test detach ----");
        int value[NUM_THREADS];
        memset(value, 0, sizeof(value));
        for (int i=0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], heavy_test_func, &value[i], 1, 1024));
            TEST_ASSERT_EQUAL(0, noza_thread_detach(th[i]));
        }
        for (int i=0; i < NUM_THREADS; i++) {
            TEST_ASSERT_NOT_EQUAL(0, noza_thread_join(th[i], NULL));
            if (value[i] == 0) {
                TEST_ASSERT_EQUAL(0, noza_thread_sleep_ms(10, NULL));
            }
        }
    }
}

// test message passing
#define SERVER_EXIT_CODE    0x0123beef
#define CLIENT_EXIT_CODE    0xdeadbeef
static int string_server_thread(void *param, uint32_t pid)
{
    for (;;) {
        noza_msg_t msg;
        TEST_ASSERT_EQUAL(0, noza_recv(&msg));
        noza_reply(&msg);
        if (strcmp((char *)msg.ptr, "kill") == 0)
            break;
    }
    return SERVER_EXIT_CODE;
}

static int string_client_thread(void *param, uint32_t mypid)
{
    noza_msg_t msg;
    char s[16];

    uint32_t pid = (uint32_t) param;
    uint32_t counter = 20;
    while (counter-->0) {
        sprintf(s, "hello %ld", counter);
        msg.to_pid = pid;
        msg.ptr = s;
        msg.size = strlen(s)+1;
        TEST_ASSERT_EQUAL(0, noza_call(&msg));
        TEST_ASSERT_EQUAL(0, noza_thread_sleep_ms(20, NULL));
    }
    strncpy(s, "kill", sizeof(s)); // terminate server
    msg.to_pid = pid;
    msg.ptr = s;
    msg.size = strlen(s)+1;
    TEST_ASSERT_EQUAL(0, noza_call(&msg));

    return CLIENT_EXIT_CODE;
}

#define ECHO_INC    0
#define ECHO_END    1
typedef struct echo_msg_s {
    uint32_t cmd;
    uint32_t value;
} echo_msg_t;

static void do_test_msg()
{
    uint32_t code, server_pid, client_pid;
    TEST_ASSERT_EQUAL(0, noza_thread_create(&server_pid, string_server_thread, NULL, 0, 1024));
    TEST_ASSERT_EQUAL(0, noza_thread_create(&client_pid, string_client_thread, (void *)server_pid, 0, 1024));
    noza_thread_join(client_pid, &code);
    TEST_ASSERT_EQUAL(CLIENT_EXIT_CODE, code);
    noza_thread_join(server_pid, &code);
    TEST_ASSERT_EQUAL(SERVER_EXIT_CODE, code);
}

int test_msg(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_msg);
    UNITY_END();
    return 0;
}

// test setjmp longjmp
#include <stdio.h>
#include <setjmp.h>

jmp_buf env;

#define JUMP_VALUE  0x12345678
void foo(void)
{
    longjmp(env, JUMP_VALUE);
}

static void do_test_setjmp()
{
    int ret = setjmp(env);
    if (ret == 0) {
        TEST_MESSAGE("initial call to setjmp");
        foo();
    } else {
        TEST_ASSERT_EQUAL(JUMP_VALUE, ret);
    }
    TEST_MESSAGE("finish test\n");
}

int test_setjmp(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_setjmp);
    UNITY_END();
    return 0;
}

// test hard fault
static int normal_task(void *param, uint32_t pid)
{
    int counter = 5;
    while (counter-->0) {
        TEST_PRINTF("normal task: %lu, count down: %ld", pid, counter);
        noza_thread_sleep_ms(500, NULL);
    }
    TEST_PRINTF("normal_task end. exit thread (%d)", pid);
    return pid;
}

static int fault_task(void *param, uint32_t pid)
{
    int counter = 3;
    while (counter-->0) {
        TEST_PRINTF("fault task: %lu, fault count down: %ld", pid, counter);
        noza_thread_sleep_ms(500, NULL);
    }
    TEST_PRINTF("RAISE fault (write memory address #00000000)!! pid=%d", pid);
    int *p = 0;
    *p = 0;
    TEST_ASSERT_EQUAL(0, 1); // never reash here
}

void do_test_hardfault()
{
    uint32_t th, fid;
    TEST_MESSAGE("test hardfault\n");
    TEST_ASSERT_EQUAL(0, noza_thread_create(&th, normal_task, NULL, 0, 1024));
    TEST_ASSERT_EQUAL(0, noza_thread_create(&fid, fault_task, NULL, 0, 1024));
    uint32_t exit_code;
    TEST_ASSERT_EQUAL(0, noza_thread_join(fid, &exit_code));
    TEST_ASSERT_EQUAL(0, noza_thread_join(th, &exit_code));
    TEST_ASSERT_EQUAL(th, exit_code);
    TEST_PRINTF("fault catch by main thread exit_code=%d", exit_code);
}

int test_hardfault(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_hardfault);
    UNITY_END();
    return 0;
}

#define ITERS 3000
static int counter = 0;
static int inc_task(void *param, uint32_t pid)
{
    mutex_t *mutex = (mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        mutex_lock(mutex);
        counter++;
        mutex_unlock(mutex);
    }
    return 0;
}

static int dec_task(void *param, uint32_t pid)
{
    mutex_t *mutex = (mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        mutex_lock(mutex);
        counter--;
        mutex_unlock(mutex);
    }
    return 0;
}

#define NUM_PAIR    4
void do_test_mutex()
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
    uint32_t inc_th[NUM_PAIR];
    uint32_t dec_th[NUM_PAIR];
    counter = 0; 
    mutex_acquire(&noza_mutex);
    for (int i = 0; i < NUM_PAIR; i++) {
        noza_thread_create(&inc_th[i], inc_task, &noza_mutex, 1, 1024);
    }
    for (int i=0; i < NUM_PAIR; i++) {
        noza_thread_create(&dec_th[i], dec_task, &noza_mutex, 1, 1024);
    }
    for (int i=0; i < NUM_PAIR; i++) {
        noza_thread_join(inc_th[i], NULL);
        noza_thread_join(dec_th[i], NULL);
    }
    TEST_ASSERT_EQUAL_INT(0, counter);
    TEST_MESSAGE("test mutex trylock");
    TEST_ASSERT_EQUAL(0, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL(EBUSY, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_unlock(&noza_mutex));
    mutex_release(&noza_mutex);
}

int test_mutex(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_mutex);
    UNITY_END();
    return 0;
}

int test_all(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_setjmp);
    RUN_TEST(do_test_thread);
    RUN_TEST(do_test_msg);
    RUN_TEST(do_test_mutex);
    RUN_TEST(do_test_hardfault);
    UNITY_END();
    return 0;
}

#include "user/console/noza_console.h"
void __attribute__((constructor(1000))) register_unittest()
{
    console_add_command("noza_unittest", test_all, "nozaos and lib, unit-test suite");
}
#endif
