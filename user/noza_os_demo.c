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
        printf("=============================================\n");
        thread_yield();
        usleep(1000000);
    }
}

void init_task(void *param)
{
    uint32_t th1, th2, th3, th4;

    th1 = thread_create(test_task, "task 1", 0);
    th2 = thread_create(test_task, "task 2", 1);
    th2 = thread_create(test_task, "task 3", 2);
    th3 = thread_create(test_task, "task 4", 3);
    test_task("task 5");
}
