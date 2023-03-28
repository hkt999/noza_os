#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"

void test_task(void *param, uint32_t pid)
{
    int do_count = rand() % 10 + 2;
    int ms = rand() % 700 + 200;
    while (do_count-->0) {
        printf("working thread id: %lu, count_down: %d, tick=%d ms\n", pid, do_count, ms);
        noza_thread_sleep(ms);
    }
    printf("test_task [pid: %lu] done\n", pid);
}

int task_demo(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <number_of_threads>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);

    if (num_threads < 2 || num_threads > 8) {
        printf("Error: The number of threads should be between 2 and 8.\n");
        return 1;
    }

    uint32_t th[num_threads];
    srand(time(0));

    for (int i = 0; i < num_threads; i++) {
        th[i] = noza_thread_create(test_task, NULL, (uint32_t)i%4);
    }

    int sleep_count = 15;
    while (sleep_count-- > 0) {
        printf("sleep count down: %ld\n", sleep_count);
        noza_thread_sleep(1000);
    }

    printf("finish test\n");
    return 0;
}

// message passing
void server_thread(void *param, uint32_t pid)
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
}

void client_thread(void *param, uint32_t my_pid)
{
    char s[16];

    uint32_t pid = (uint32_t) param;
    uint32_t counter = 0;
    while (counter-->0) {
        sprintf(s, "hello %ld", counter++);
        noza_msg_t msg = {.pid=pid, .ptr=s, .size=strlen(s)+1};
        printf("client call (%s)\n", s);
        int code = noza_call(&msg);
        printf("    client return %d\n", code);
        noza_thread_sleep(300);
    }
    strncpy(s, "kill", sizeof(s));
    noza_msg_t msg = {.pid=pid, .ptr=s, .size=strlen(s)+1};
}

int message_demo(int argc, char **argv)
{
    uint32_t pid = noza_thread_create(server_thread, NULL, 0);
    printf("** server pid: %ld\n", pid);
    printf("** client start\n");
    client_thread((void *)pid, 0);
    noza_thread_join(pid);

    printf("test finish\n");
    return 0;
}

void thread_working(void *param, uint32_t pid)
{
    uint32_t master = (uint32_t) param;
    uint32_t counter = 5;
    uint32_t sleep_ms = rand() % 500 + 500;
    while (counter-->0) {
        printf("thread_id: %lu, join count down: %ld\n", pid, counter);
        noza_thread_sleep(1000);
    }
    printf("enter join state\n");
    noza_thread_join(master);
    printf("-- detect master thread terminated\n");
}

void thread_master(void *param, uint32_t pid)
{
    uint32_t counter = 10;
    while (counter-- > 0) {
        printf("master count down: %ld\n", counter);
        noza_thread_sleep(1000);
    }
    printf("** master thread terminated\n");
}

int thread_join_demo(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: test_join <thread count>\n");
        return 0;
    }
    srand(time(0));
    int count = atoi(argv[1]);
    if (count > 8 || count < 1) {
        printf("thread count must be 1~8\n");
        return 0;
    }
    printf("create %d working threads\n", count);
    uint32_t master = noza_thread_create(thread_master, NULL, 0);
    for (int i=0; i<count; i++) {
        noza_thread_create(thread_working, (void *)master, 0);
    }
    noza_thread_join(master); // main thread also wait for master thread
    printf("finish: main thread join master thread\n");
    return 0;
}

#include "user/console/console.h"
builtin_cmd_t builtin_cmds[] = {
    {"test_task", task_demo, "a demo program for task creation and scheduling"},
    {"test_msg", message_demo, "a demo program for message passing"},
    {"test_join", thread_join_demo, "a demo program for thread synchronization join"},
    {NULL, NULL}
};

void __user_start()
{
    uint32_t th = noza_thread_create(console_start, &builtin_cmds[0], 0);

    noza_thread_join(th);
    noza_thread_terminate();
}
