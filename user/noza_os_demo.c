#include <stdio.h>
#include "syslib.h"

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

void init_task(void *param)
{
    uint32_t th1, th2, th3, th4;
    noza_thread_sleep(2000);

    th1 = noza_thread_create(test_task, "task 1", 0);
    noza_thread_sleep(100);
    th2 = noza_thread_create(test_task, "task 2", 1);
    noza_thread_sleep(100);
    th3 = noza_thread_create(test_task, "task 3", 2);
    noza_thread_sleep(100);
    th4 = noza_thread_create(test_task, "task 4", 3);
    noza_thread_sleep(100);
    while (1) {
        printf("%d, %d, %d, %d\n", th1, th2, th3, th4);
        noza_thread_sleep(700);
    }
}
