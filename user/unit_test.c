//#ifdef NOZAOS_UNITTEST
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "posix/bits/signum.h"
#include "kernel/noza_config.h"
#include "errno.h"
#include <service/mutex/mutex_client.h>

#define UNITY_INCLUDE_CONFIG_H
#include "unity.h"


void setUp()
{
}

void tearDown()
{
}

static int test_task(void *param, uint32_t pid)
{
    int do_count = rand() % 5 + 2;
    int ms = rand() % 50 + 50;
    while (do_count-->0) {
        int ret = noza_thread_sleep_ms(ms, NULL);
        if (ret != 0) {
            TEST_ASSERT_EQUAL(EINTR, ret);
            break;
        }
    }
    return pid;
}

static int yield_test_func(void *param, uint32_t pid)
{
    #define YIELD_ITER  10000
    uint32_t value = 0;
    for (int i=0; i<YIELD_ITER; i++) {
        value = value + i;
        TEST_ASSERT_EQUAL(0, noza_thread_sleep_us(0, NULL));
    }

    return value;
}

static int heavy_test_func(void *param, uint32_t pid)
{
    #define HEAVY_ITER  10000000
    uint32_t value = 0;
    for (int i=0; i<HEAVY_ITER; i++) {
        value = value + i;
    }

    return value;
}

static void do_test_thread()
{
    #define NUM_THREADS 8
    #define NUM_LOOP    8
    uint32_t th[NUM_THREADS];
    uint32_t pid;
    noza_thread_self(&pid);

    //for (int loop = 0; loop < NUM_LOOP; loop++) {
    for (;;) {
        srand(time(0));
#if 1
        TEST_MESSAGE("---- test thread creation with random priority and sleep time ----");
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], test_task, NULL, (uint32_t)i%NOZA_OS_PRIORITY_LIMIT, 1024));
            TEST_ASSERT_NOT_EQUAL(0, th[i]);
        }

        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_PRINTF("***** join thread me: %d, pid: %d", pid, th[i]);
            TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
            TEST_ASSERT_EQUAL(th[i], exit_code);
        }
#endif

#if 1
        TEST_MESSAGE("---- test thread creation with random priority and signal in 100ms ----");
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], test_task, NULL, (uint32_t)i%NOZA_OS_PRIORITY_LIMIT, 1024));
            TEST_ASSERT_NOT_EQUAL(0, th[i]);
        }
        noza_thread_sleep_ms(50, NULL);
        TEST_MESSAGE("send signal (kill) to all threads");
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t sig = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_kill(th[i], SIGALRM));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
            TEST_PRINTF("thread %d exit code: %ld", i, exit_code);
            TEST_ASSERT_EQUAL(th[i], exit_code);
        }
#endif

#if 1
        TEST_MESSAGE("---- test heavy loading ----");
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], heavy_test_func, NULL, 1, 1024));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
            TEST_ASSERT_EQUAL_UINT(2280707264, exit_code);
        }
#endif

#if 1
        TEST_MESSAGE("---- test yield ----");
        for (int i = 0; i < NUM_THREADS; i++) {
            TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], yield_test_func, NULL, 1, 1024));
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            uint32_t exit_code = 0;
            TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
            TEST_ASSERT_EQUAL(49995000, exit_code);
        }
#endif
    }
}

int test_thread(int argc, char **argv)
{
    UNITY_BEGIN();
    do_test_thread();
    UNITY_END();
}

// test message passing
#define SERVER_EXIT_CODE    0x0123beef
#define CLIENT_EXIT_CODE    0xdeadbeef
static int string_server_thread(void *param, uint32_t pid)
{
    for (;;) {
        noza_msg_t msg;
        TEST_ASSERT_EQUAL(0, noza_recv(&msg));
        TEST_PRINTF("server recv msg: [%s]", (char *)msg.ptr);
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
    strncpy(s, "kill", sizeof(s));
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

static int echo_server_thread(void *param, uint32_t mypid)
{
    noza_msg_t msg;

    for (;;) {
        noza_msg_t msg;
        int ret = noza_recv(&msg);
        echo_msg_t *echo_msg = (echo_msg_t *)msg.ptr;
        switch (echo_msg->cmd) {
            case ECHO_INC:
                echo_msg->value++;
                break;
        }
        noza_reply(&msg);
        if (echo_msg->cmd == ECHO_END)
            break;
    }
    return SERVER_EXIT_CODE;
}

static int echo_client_thread(void *param, uint32_t mypid)
{
    noza_msg_t msg;
    echo_msg_t echo_msg;
    uint32_t pid = (uint32_t) param;
    uint32_t counter = 200;
    msg.to_pid = pid;
    msg.ptr = &echo_msg;
    msg.size = sizeof(echo_msg);

    while (counter-->0) {
        int test_value = rand();
        echo_msg.cmd = ECHO_INC;
        echo_msg.value = test_value;
        printf("noza call\n");
        TEST_ASSERT_EQUAL(0, noza_call(&msg));
        printf("after noza call\n");
        TEST_ASSERT_EQUAL(test_value+1, echo_msg.value);
    }
    echo_msg.cmd = ECHO_END;
    TEST_ASSERT_EQUAL(0, noza_call(&msg));
    return CLIENT_EXIT_CODE;
}

static void do_test_msg()
{
    uint32_t code, server_pid, client_pid;
    TEST_ASSERT_EQUAL(0, noza_thread_create(&server_pid, string_server_thread, NULL, 0, 2048));
    TEST_ASSERT_EQUAL(0, noza_thread_create(&client_pid, string_client_thread, (void *)server_pid, 0, 2048));
    noza_thread_join(client_pid, &code);
    TEST_ASSERT_EQUAL(CLIENT_EXIT_CODE, code);
    noza_thread_join(server_pid, &code);
    TEST_ASSERT_EQUAL(SERVER_EXIT_CODE, code);

#if 0
    TEST_ASSERT_EQUAL(0, noza_thread_create(&server_pid, echo_server_thread, NULL, 0, 1024));
    TEST_ASSERT_EQUAL(0, noza_thread_create(&client_pid, echo_client_thread, (void *)server_pid, 0, 1024));
    noza_thread_join(client_pid, &code);
    TEST_ASSERT_EQUAL(CLIENT_EXIT_CODE, code);
    noza_thread_join(server_pid, &code);
    TEST_ASSERT_EQUAL(SERVER_EXIT_CODE, code);
#endif
}

int test_msg(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_msg);
    UNITY_END();
    return 0;
}

//
static int thread_working(void *param, uint32_t pid)
{
    uint32_t p = (uint32_t)param;
    uint32_t counter = 5;
    uint32_t sleep_ms = rand() % 500 + 500;
    while (counter-->0) {
        TEST_PRINTF("thread_id: %lu, join count down: %ld", pid, counter);
        noza_thread_sleep_ms(sleep_ms, NULL);
    }
    return p;
}

static void do_test_join()
{
    #define TEST_THREAD_COUNT   8
    uint32_t th[TEST_THREAD_COUNT];
    srand(time(0));
    TEST_PRINTF("create %d working threads\n", TEST_THREAD_COUNT);
    for (int i=0; i<TEST_THREAD_COUNT; i++) {
        TEST_ASSERT_EQUAL(0, noza_thread_create(&th[i], thread_working, (void *)i, 0, 1024));
        TEST_ASSERT_NOT_EQUAL(0, th[i]);
    }

    for (int i=0; i<TEST_THREAD_COUNT; i++) {
        uint32_t exit_code;
        TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
        TEST_ASSERT_EQUAL(i, exit_code);
    }
}

int test_join(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_join);
    UNITY_END();
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
    int counter = 10;
    while (counter-->0) {
        TEST_PRINTF("normal task: %lu, count down: %ld", pid, counter);
        noza_thread_sleep_ms(500, NULL);
    }
    TEST_PRINTF("normal_task end. exit thread (%d)", pid);
    return pid;
}

static int fault_task(void *param, uint32_t pid)
{
    int counter = 10;
    while (counter-->0) {
        TEST_PRINTF("fault task: %lu, fault count down: %ld", pid, counter);
        noza_thread_sleep_ms(500, NULL);
    }
    TEST_PRINTF("raise fault (write memory address #00000000)!!");
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
    TEST_PRINTF("fault catch by main thread exit_code=%d", exit_code);
}

int test_hardfault(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_hardfault);
    UNITY_END();
    return 0;
}

#define ITERS 100
static int counter = 0;
static int inc_task(void *param, uint32_t pid)
{
    mutex_t *mutex = (mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        printf("--a1\n");
        mutex_lock(mutex);
        printf("%d inc lock\n", pid);
        counter++;
        printf("%d inc unlock %d\n", pid, counter);
        mutex_unlock(mutex);
        printf("--b1\n");
    }
    printf("dec task %d finish\n");
    return 0;
}

static int dec_task(void *param, uint32_t pid)
{
    mutex_t *mutex = (mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        mutex_lock(mutex);
        printf("%d dec lock\n", pid);
        counter--;
        printf("%d dec unlock %d\n", pid, counter);
        mutex_unlock(mutex);
    }
    printf("dec task %d finish\n");
    return 0;
}

#define NUM_PAIR    1
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

    uint32_t th[NUM_PAIR*2];
    counter = 0; 
    mutex_acquire(&noza_mutex);
    for (int i = 0; i<NUM_PAIR; i++) {
        noza_thread_create(&th[i], inc_task, &noza_mutex, 1, 1024);
    }
    /*
    for (int i=NUM_PAIR; i<NUM_PAIR*2; i++) {
        noza_thread_create(&th[i], dec_task, &noza_mutex, 1, 1024);
    }
    */
    for (int i=0; i<NUM_PAIR; i++) {
        TEST_PRINTF("join %d\n", i);
        noza_thread_join(th[i], NULL);
    }
    mutex_release(&noza_mutex);
    TEST_ASSERT_EQUAL_INT(0, counter);
    #if 0
    TEST_MESSAGE("test mutex trylock");
    TEST_ASSERT_EQUAL(0, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL(EBUSY, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL(0, mutex_unlock(&noza_mutex));
    #endif
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
    RUN_TEST(do_test_join);
    RUN_TEST(do_test_mutex);
    //RUN_TEST(do_test_hardfault);
    UNITY_END();
    return 0;
}
//#endif // end of unittest
