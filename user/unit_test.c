#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "unity.h"

#ifdef NOZAOS_UNITTEST
int test_task(void *param, uint32_t pid)
{
    int do_count = rand() % 10 + 2;
    int ms = rand() % 700 + 200;
    while (do_count-->0) {
        printf("working thread id: %lu, count_down: %d, tick=%d ms\n", pid, do_count, ms);
        TEST_ASSERT_EQUAL(0, noza_thread_sleep(ms));
    }
    printf("test_task [pid: %lu] done\n", pid);
    return pid;
}

int task_test(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <number_of_threads>\n", argv[0]);
        return 1;
    }
    int num_threads = atoi(argv[1]);

    if (num_threads < 2 || num_threads > 8) {
        printf("Error: The number of threads should be between 2 and 8.\n");
        return 1;
    }

    UNITY_BEGIN();
    uint32_t th[num_threads];
    srand(time(0));

    for (int i = 0; i < num_threads; i++) {
        th[i] = noza_thread_create(test_task, NULL, (uint32_t)i%4, 1024);
        TEST_ASSERT_NOT_EQUAL(0, th[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        uint32_t exit_code = 0;
        TEST_ASSERT_EQUAL(0, noza_thread_join(th[i], &exit_code));
        TEST_ASSERT_EQUAL(th[i], exit_code);
    }
    UNITY_END();
    return 0;
}

// message passing
int server_thread(void *param, uint32_t pid)
{
    for (;;) {
        noza_msg_t msg;
        int ret = noza_recv(&msg);
        if (ret == 0) {
            printf("server got msg: %s\n", (char *)msg.ptr);
            noza_reply(&msg);
            if (strcmp((char *)msg.ptr, "kill") == 0)
                break;
        } else {
            printf("server recv error: %d\n", ret);
        }
    }
    return 0;
}

void client_thread(void *param, uint32_t mypid)
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
        printf("client call (%s)\n", s);
        int code = noza_call(&msg);
        printf("    client return %d\n", code);
        noza_thread_sleep(300);
    }
    strncpy(s, "kill", sizeof(s));
    msg.pid = pid;
    msg.ptr = s;
    msg.size = strlen(s)+1;
    int code = noza_call(&msg);
    printf("    client return %d\n", code);
}

int message_test(int argc, char **argv)
{
    uint32_t code;
    UNITY_BEGIN();
    uint32_t pid = noza_thread_create(server_thread, NULL, 0, 1024);
    printf("** server pid: %ld\n", pid);
    printf("** client start\n");
    client_thread((void *)pid, 0);
    noza_thread_join(pid, &code);
    TEST_ASSERT_EQUAL(pid, code);
    printf("test finish code=%d\n", pid);
    UNITY_END();
    return 0;
}

int thread_working(void *param, uint32_t pid)
{
    uint32_t exit_code;
    uint32_t master = (uint32_t) param;
    uint32_t counter = 5;
    uint32_t sleep_ms = rand() % 500 + 500;
    while (counter-->0) {
        printf("thread_id: %lu, join count down: %ld\n", pid, counter);
        noza_thread_sleep(1000);
    }
    printf("enter join state\n");
    noza_thread_join(master, &exit_code);
    printf("-- detect master thread terminated exit_code=%d\n", exit_code);

    return pid;
}

int thread_master(void *param, uint32_t pid)
{
    uint32_t counter = 10;
    while (counter-- > 0) {
        printf("master count down: %ld\n", counter);
        noza_thread_sleep(1000);
    }
    printf("** master thread terminated (%d)\n", pid);
    return pid;
}

int _thread_join_test(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: test_join <thread count>\n");
        return 0;
    }
    UNITY_BEGIN();
    srand(time(0));
    int count = atoi(argv[1]);
    if (count > 8 || count < 1) {
        printf("thread count must be 1~8\n");
        return 0;
    }
    printf("create %d working threads\n", count);
    uint32_t master = noza_thread_create(thread_master, NULL, 0, 1024);
    for (int i=0; i<count; i++) {
        noza_thread_create(thread_working, (void *)master, 0, 1024);
    }
    uint32_t exit_code;
    noza_thread_join(master, &exit_code);
    TEST_ASSERT_EQUAL(master, exit_code);
    UNITY_END();
    return 0;
}

int thread_join_test(int argc, char **argv)
{
    UNITY_BEGIN();
    //RUN_TEST(_thread_join_test);
    UNITY_END();
}

#include <stdio.h>
#include <setjmp.h>

jmp_buf env;

#define JUMP_VALUE 123
void foo(void)
{
    printf("entering foo\n");
    longjmp(env, JUMP_VALUE);
    printf("exiting foo\n");
}

int setjmp_test(int argc, char **argv)
{
    UNITY_BEGIN();
    int ret = setjmp(env);
    if (ret == 0) {
        printf("initial call to setjmp\n");
        foo();
    } else {
        printf("returned from longjmp with value %d\n", ret);
        TEST_ASSERT_EQUAL(JUMP_VALUE, ret);
    }
    printf("finish test\n");
    UNITY_END();

    return 0;
}

int normal_task(void *param, uint32_t pid)
{
    int counter = 10;
    while (counter-->0) {
        printf("normal task: %lu, count down: %ld\n", pid, counter);
        noza_thread_sleep(500);
    }
    printf("normal_task end. finish test\n");
    return pid;
}

int fault_task(void *param, uint32_t pid)
{
    int counter = 10;
    while (counter-->0) {
        printf("fault task: %lu, fault count down: %ld\n", pid, counter);
        noza_thread_sleep(500);
    }
    printf("raise fault (write memory address #00000000)!!\n");
    int *p = 0;
    *p = 0;
    printf("not reach here\n");
}

int hardfault_test(int argc, char **argv)
{
    printf("test hardfault\n");
    noza_thread_create(normal_task, NULL, 0, 1024);
    uint32_t fid = noza_thread_create(fault_task, NULL, 0, 1024);
    uint32_t exit_code;
    noza_thread_join(fid, &exit_code);
    printf("fault catch by main thread exit_code=%d\n", exit_code);
}

#endif // end of unittest
