//#ifdef NOZAOS_UNITTEST
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"

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
    int do_count = rand() % 10 + 2;
    int ms = rand() % 700 + 200;
    while (do_count-->0) {
        TEST_PRINTF("working thread id: %lu, count_down: %d, tick=%d ms", pid, do_count, ms);
        TEST_ASSERT_EQUAL(0, noza_thread_sleep(ms));
    }
    TEST_PRINTF("test_task [pid: %lu] done", pid);
    return pid;
}

static void do_test_thread()
{
    #define NUM_THREADS 8
    uint32_t th[NUM_THREADS];
    srand(time(0));

    for (int i = 0; i < NUM_THREADS; i++) {
        th[i] = noza_thread_create(test_task, NULL, (uint32_t)i%4, 1024);
        TEST_ASSERT_NOT_EQUAL(0, th[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        uint32_t exit_code = 0;
        TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
        TEST_ASSERT_EQUAL(th[i], exit_code);
    }
}

int test_thread(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_thread);
    UNITY_END();
}

// test message passing
#define SERVER_EXIT_CODE    0x0123beef
static int server_thread(void *param, uint32_t pid)
{
    for (;;) {
        noza_msg_t msg;
        int ret = noza_recv(&msg);
        if (ret == 0) {
            TEST_PRINTF("server got msg: %s", (char *)msg.ptr);
            noza_reply(&msg);
            if (strcmp((char *)msg.ptr, "kill") == 0)
                break;
        } else {
            TEST_PRINTF("server recv error: %d", ret);
        }
    }
    return SERVER_EXIT_CODE;
}

static void client_thread(void *param, uint32_t mypid)
{
    noza_msg_t msg;
    char s[16];

    uint32_t pid = (uint32_t) param;
    uint32_t counter = 20;
    while (counter-->0) {
        sprintf(s, "hello %ld", counter);
        msg.pid = pid;
        msg.ptr = s;
        msg.size = strlen(s)+1;
        TEST_PRINTF("client call (%s)", s);
        TEST_ASSERT_EQUAL(0, noza_call(&msg));
        noza_thread_sleep(300);
    }
    strncpy(s, "kill", sizeof(s));
    msg.pid = pid;
    msg.ptr = s;
    msg.size = strlen(s)+1;
    TEST_ASSERT_EQUAL(0, noza_call(&msg));
}

static void do_test_msg()
{
    uint32_t code;
    uint32_t pid = noza_thread_create(server_thread, NULL, 0, 1024);
    TEST_PRINTF("** server pid: %ld", pid);
    TEST_MESSAGE("** client start");
    client_thread((void *)pid, 0);
    noza_thread_join(pid, &code);
    TEST_ASSERT_EQUAL(SERVER_EXIT_CODE, code);
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
        noza_thread_sleep(sleep_ms);
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
        th[i]= noza_thread_create(thread_working, (void *)i, 0, 1024);
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
        noza_thread_sleep(500);
    }
    TEST_PRINTF("normal_task end. exit thread (%d)", pid);
    return pid;
}

static int fault_task(void *param, uint32_t pid)
{
    int counter = 10;
    while (counter-->0) {
        TEST_PRINTF("fault task: %lu, fault count down: %ld", pid, counter);
        noza_thread_sleep(500);
    }
    TEST_PRINTF("raise fault (write memory address #00000000)!!");
    int *p = 0;
    *p = 0;
    TEST_ASSERT_EQUAL(0, 1); // never reash here
}

void do_test_hardfault()
{
    TEST_MESSAGE("test hardfault\n");
    noza_thread_create(normal_task, NULL, 0, 1024);
    uint32_t fid = noza_thread_create(fault_task, NULL, 0, 1024);
    uint32_t exit_code;
    TEST_ASSERT_EQUAL(0, noza_thread_join(fid, &exit_code));
    TEST_PRINTF("fault catch by main thread exit_code=%d", exit_code);
}

int test_hardfault(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_hardfault);
    UNITY_END();
}

int test_all(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(do_test_setjmp);
    RUN_TEST(do_test_thread);
    RUN_TEST(do_test_msg);
    RUN_TEST(do_test_join);
    //RUN_TEST(do_test_hardfault);
    UNITY_END();
    return 0;
}
//#endif // end of unittest
