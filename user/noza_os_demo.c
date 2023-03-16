#include <stdio.h>
#include "kernel/syscall.h"
//#include "pico/stdlib.h"

extern void asm_noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
uint32_t noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2)
{
    uint32_t ret_addr;
    asm_noza_syscall(r0, r1, r2, (uint32_t)&ret_addr);

    return ret_addr;
}

uint32_t app_sleep_ms(uint32_t ticks)
{
    return noza_syscall(NSC_SLEEP, ticks, 0);
}

uint32_t app_create_thread(void (*entry)(void *param), void *param, uint32_t pri)
{
    return noza_syscall(NSC_CREATE_THREAD, (uint32_t) entry, (uint32_t) param);
}

void test_task(void *param)
{
    static int counter = 10;
    int do_count = counter;
    counter += 10;
    const char *str = (const char *) param;
    while (--do_count>0) {
        printf("%d, %s\n", do_count, str);
        printf("=============================================\n");
        app_sleep_ms(1000);
    }
}

void init_task(void *param)
{
    uint32_t th1, th2, th3, th4;

    th1 = app_create_thread(test_task, "task 1", 0);
    th2 = app_create_thread(test_task, "task 2", 1);
    th2 = app_create_thread(test_task, "task 3", 2);
    th3 = app_create_thread(test_task, "task 4", 3);
    while (1) {
        app_sleep_ms(800);
    }
}
