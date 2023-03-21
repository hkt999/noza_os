#include <stdio.h>
#include "syslib.h"
#include <string.h>

void test_task(void *param)
{
    static int counter = 10;
    int do_count = counter;
    counter += 10;
    const char *str = (const char *) param;
    while (--do_count>0) {
        printf("%d, %s\n", do_count, str);
        noza_thread_yield();
        noza_thread_sleep(1000);
    }
}

void task_demo()
{
    uint32_t th1, th2, th3, th4;
    noza_thread_sleep(2000);

    th1 = noza_thread_create(test_task, "task 1", 0);
    th2 = noza_thread_create(test_task, "task 2", 1);
    th3 = noza_thread_create(test_task, "task 3", 2);
    th4 = noza_thread_create(test_task, "task 4", 3);
    while (1) {
        printf("%ld, %ld, %ld, %ld\n", th1, th2, th3, th4);
        noza_thread_sleep(700);
    }
}

void server(void *param)
{
    for (;;) {
        noza_msg_t msg;
        if (noza_recv(&msg) == 0) {
            printf("msg: %s\n", (char *)msg.ptr);
            noza_reply(&msg);
        }
    }
}

void client(void *param)
{
    char s[16];

    uint32_t pid = (uint32_t) param;
    uint32_t counter = 0;
    for (;;) {
        sprintf(s, "hello %ld", counter++);
        noza_msg_t msg = {.pid = pid, .ptr = (void *)s, .size = strlen(s) + 1};
        noza_call(&msg);
        noza_thread_sleep(1000);
    }
}

void message_demo()
{
    uint32_t pid = noza_thread_create(server, NULL, 0);
    printf("** server pid: %ld\n", pid);
    printf("** start client\n");
    client((void *)pid);
}

void thread_working(void *param)
{
    uint32_t master = (uint32_t) param;
    uint32_t counter = 5;
    while (counter-->0) {
        printf("join count down: %ld\n", counter);
        noza_thread_sleep(1000);
    }
    printf("enter join state\n");
    noza_thread_join(master);
    printf("-- master thread terminated\n");
    noza_thread_terminate();
}

void thread_master(void *param)
{
    uint32_t counter = 10;
    while (counter-- > 0) {
        printf("master count down: %ld\n", counter);
        noza_thread_sleep(1000);
    }
    printf("** master thread terminated\n");
    noza_thread_terminate();
}

void thread_join_demo()
{
    uint32_t master = noza_thread_create(thread_master, NULL, 0);
    noza_thread_create(thread_working, (void *)master, 0);
    noza_thread_create(thread_working, (void *)master, 0);
    for (;;) {
        noza_thread_sleep(1000);
        printf(" -- main loop tick --\n");
    }

    noza_thread_terminate();
}

void __user_start()
{
    // task_demo();
    int counter = 4;
    while (counter-->0) {
        noza_thread_sleep(1000);
        printf("count down: %d\n", counter);
    }
    //message_demo();
    thread_join_demo();
    noza_thread_terminate();
}
