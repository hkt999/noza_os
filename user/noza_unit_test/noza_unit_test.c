#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "nozaos.h"
#include "spinlock.h"
#include "posix/bits/signum.h"
#include "kernel/noza_config.h"
#include "posix/errno.h"
#include <service/name_lookup/name_lookup_client.h>
#include <service/sync/sync_client.h>

#define UNITY_INCLUDE_CONFIG_H
#include "unity.h"

static int test_task(void *param, uint32_t pid)
{
    int do_count = rand() % 5 + 2;
    int ms = rand() % 50 + 50;
    while (do_count-->0) {
        uint32_t lpid;
        TEST_ASSERT_EQUAL_INT(0, noza_thread_self(&lpid));
        TEST_ASSERT_EQUAL_INT(pid, lpid);
        int ret = noza_thread_sleep_ms(ms, NULL);
        if (ret != 0) {
            TEST_ASSERT_NOT_EQUAL(0, ret);
            break;
        }
    }
    return pid;
}

static int yield_test_func(void *param, uint32_t pid)
{
    #define YIELD_ITER  100
    uint32_t value = 0;
    for (int i=0; i<YIELD_ITER; i++) {
        value = value + i;
        if (param == NULL) {
            TEST_ASSERT_EQUAL_INT(0, noza_thread_sleep_us(0, NULL));
        }
    }

    return value;
}

static int heavy_test_func(void *param, uint32_t pid)
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

    return value;
}

#define NUM_THREADS 8
static void test_thread_create_join()
{
    uint32_t th[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th[i], test_task, NULL, (uint32_t)i%NOZA_OS_PRIORITY_LIMIT, 1024));
        TEST_ASSERT_NOT_EQUAL(0, th[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        uint32_t exit_code = 0;
        TEST_ASSERT_EQUAL_INT(0, noza_thread_join(th[i], &exit_code));
        TEST_ASSERT_EQUAL_INT(th[i], exit_code);
    }
}

static void test_thread_sleep_and_signal()
{
    uint32_t th[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th[i], test_task, NULL, (uint32_t)i%NOZA_OS_PRIORITY_LIMIT, 1024));
    }
    noza_thread_sleep_ms(100, NULL);
    for (int i = 0; i < NUM_THREADS; i++) {
        uint32_t sig = 0;
        noza_thread_kill(th[i], SIGALRM);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        uint32_t exit_code = 0;
        TEST_ASSERT_EQUAL_INT(0, noza_thread_join(th[i], &exit_code));
        TEST_ASSERT_EQUAL_INT(th[i], exit_code);
    }
}

static void test_heavy_loading_thread()
{
    uint32_t pid;
    uint32_t value_heavy = heavy_test_func(NULL, pid);
    uint32_t th[NUM_THREADS];

    noza_thread_self(&pid);
    for (int i = 0; i < NUM_THREADS; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th[i], heavy_test_func, NULL, 1, 1024));
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        uint32_t exit_code = 0;
        TEST_ASSERT_EQUAL_INT(0, noza_thread_join(th[i], &exit_code));
        TEST_ASSERT_EQUAL_UINT(value_heavy, exit_code);
    }
}

static void test_thread_yield()
{
    uint32_t pid;
    uint32_t th[NUM_THREADS];

    noza_thread_self(&pid);
    uint32_t value_yield = yield_test_func(&pid, pid);
    for (int i = 0; i < NUM_THREADS; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th[i], yield_test_func, NULL, 1, 1024));
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        uint32_t exit_code = 0;
        TEST_ASSERT_EQUAL_INT(0, noza_thread_join(th[i], &exit_code));
        TEST_ASSERT_EQUAL_INT(value_yield, exit_code);
    }
}

static void test_noza_thread_detach()
{
    int value[NUM_THREADS];
    uint32_t th[NUM_THREADS];

    memset(value, 0, sizeof(value));
    for (int i=0; i < NUM_THREADS; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th[i], heavy_test_func, &value[i], 1, 1024));
        TEST_ASSERT_EQUAL_INT(0, noza_thread_detach(th[i]));
    }
    for (int i=0; i < NUM_THREADS; i++) {
        TEST_ASSERT_NOT_EQUAL(0, noza_thread_join(th[i], NULL));
        if (value[i] == 0) {
            TEST_ASSERT_EQUAL_INT(0, noza_thread_sleep_ms(10, NULL));
        }
    }
}

static uint64_t time64_to_ns(const noza_time64_t *ts)
{
    return ((uint64_t)ts->high << 32) | ts->low;
}

static uint64_t monotonic_ns(void)
{
    noza_time64_t ts;
    TEST_ASSERT_EQUAL_INT(0, noza_clock_gettime(NOZA_CLOCK_MONOTONIC, &ts));
    return time64_to_ns(&ts);
}

typedef struct {
    volatile uint32_t futex_word;
    int result;
} futex_ctx_t;

static int futex_wait_worker(void *param, uint32_t pid)
{
    futex_ctx_t *ctx = (futex_ctx_t *)param;
    ctx->result = noza_futex_wait((uint32_t *)&ctx->futex_word, 0, -1);
    return ctx->result;
}

static int futex_timeout_worker(void *param, uint32_t pid)
{
    return noza_futex_wait((uint32_t *)param, 0, 20000);
}

static void test_futex_wait_wake(void)
{
    futex_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    uint32_t th;
    TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th, futex_wait_worker, &ctx, 1, 1024));
    TEST_ASSERT_EQUAL_INT(0, noza_thread_sleep_ms(5, NULL));
    ctx.futex_word = 1;
    TEST_ASSERT_EQUAL_INT(1, noza_futex_wake((uint32_t *)&ctx.futex_word, 1));
    uint32_t exit_code = 0;
    TEST_ASSERT_EQUAL_INT(0, noza_thread_join(th, &exit_code));
    TEST_ASSERT_EQUAL_INT(0, exit_code);
}

static void test_futex_timeout(void)
{
    volatile uint32_t futex_word = 0;
    uint32_t th;
    TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th, futex_timeout_worker, (void *)&futex_word, 1, 1024));
    uint32_t exit_code = 0;
    TEST_ASSERT_EQUAL_INT(0, noza_thread_join(th, &exit_code));
    TEST_ASSERT_EQUAL_INT(ETIMEDOUT, exit_code);
}

static void test_timer_one_shot(void)
{
    uint32_t timer_id = 0;
    TEST_ASSERT_EQUAL_INT(0, noza_timer_create(&timer_id));
    uint64_t before_ns = monotonic_ns();
    TEST_ASSERT_EQUAL_INT(0, noza_timer_arm(timer_id, 20000, 0));
    TEST_ASSERT_EQUAL_INT(0, noza_timer_wait(timer_id, -1));
    uint64_t after_ns = monotonic_ns();
    TEST_ASSERT_TRUE(after_ns - before_ns >= 20000ULL * 1000ULL);
    TEST_ASSERT_EQUAL_INT(0, noza_timer_delete(timer_id));
}

static void test_timer_periodic(void)
{
    uint32_t timer_id = 0;
    TEST_ASSERT_EQUAL_INT(0, noza_timer_create(&timer_id));
    TEST_ASSERT_EQUAL_INT(0, noza_timer_arm(timer_id, 10000, NOZA_TIMER_FLAG_PERIODIC));
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_timer_wait(timer_id, 50000));
    }
    TEST_ASSERT_EQUAL_INT(0, noza_timer_cancel(timer_id));
    TEST_ASSERT_EQUAL_INT(0, noza_timer_delete(timer_id));
}

static void test_timer_cancel(void)
{
    uint32_t timer_id = 0;
    TEST_ASSERT_EQUAL_INT(0, noza_timer_create(&timer_id));
    TEST_ASSERT_EQUAL_INT(0, noza_timer_arm(timer_id, 50000, 0));
    TEST_ASSERT_EQUAL_INT(0, noza_timer_cancel(timer_id));
    TEST_ASSERT_EQUAL_INT(ETIMEDOUT, noza_timer_wait(timer_id, 10000));
    TEST_ASSERT_EQUAL_INT(0, noza_timer_delete(timer_id));
}

static void test_clock_monotonic(void)
{
    uint64_t start_ns = monotonic_ns();
    TEST_ASSERT_EQUAL_INT(0, noza_thread_sleep_ms(5, NULL));
    uint64_t end_ns = monotonic_ns();
    TEST_ASSERT_TRUE(end_ns > start_ns);
}

typedef struct {
    volatile uint32_t futex_word;
    uint32_t pending_mask;
    int wait_result;
} signal_ctx_t;

static int signal_wait_thread(void *param, uint32_t pid)
{
    signal_ctx_t *ctx = (signal_ctx_t *)param;
    ctx->wait_result = noza_futex_wait((uint32_t *)&ctx->futex_word, 0, -1);
    ctx->pending_mask = noza_signal_take();
    return ctx->wait_result;
}

static void test_signal_interrupts_futex(void)
{
    signal_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    uint32_t th;
    TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th, signal_wait_thread, &ctx, 1, 1024));
    TEST_ASSERT_EQUAL_INT(0, noza_thread_sleep_ms(5, NULL));
    TEST_ASSERT_EQUAL_INT(0, noza_signal_send(th, SIGUSR1));
    uint32_t exit_code = 0;
    TEST_ASSERT_EQUAL_INT(0, noza_thread_join(th, &exit_code));
    TEST_ASSERT_EQUAL_INT(EINTR, exit_code);
    TEST_ASSERT_NOT_EQUAL(0, ctx.pending_mask & (1u << (SIGUSR1 - 1)));
}

// test message passing
#define SERVER_EXIT_CODE    0x0123beef
#define CLIENT_EXIT_CODE    0x0eadbeef
static int string_server_thread(void *param, uint32_t pid)
{
    for (;;) {
        noza_msg_t msg;
        TEST_ASSERT_EQUAL_INT(0, noza_recv(&msg));
        if (strcmp((char *)msg.ptr, "kill") == 0) {
            noza_reply(&msg);
            break;
        }
        noza_reply(&msg);
    }
    return SERVER_EXIT_CODE;
}

static int string_client_thread(void *param, uint32_t mypid)
{
    noza_msg_t msg;
    static char s[16];

    uint32_t pid = (uint32_t) param;
    uint32_t counter = 20;
    while (counter-->0) {
        snprintf(s, sizeof(s), "hello %ld", counter);
        msg.to_vid = pid;
        msg.ptr = s;
        msg.size = strlen(s)+1;
        TEST_ASSERT_EQUAL_INT(0, noza_call(&msg));
        TEST_ASSERT_EQUAL_INT(0, noza_thread_sleep_ms(10, NULL));
    }
    strncpy(s, "kill", sizeof(s)); // terminate server
    msg.to_vid = pid;
    msg.ptr = s;
    msg.size = strlen(s)+1;
    TEST_ASSERT_EQUAL_INT(0, noza_call(&msg));

    noza_thread_sleep_ms(20, NULL);
    // test noza_call_error
    strncpy(s, "hello end", sizeof(s));
    msg.to_vid = pid;
    msg.ptr = s;
    msg.size = strlen(s) + 1;
    TEST_ASSERT_EQUAL_INT(ESRCH, noza_call(&msg));

    return CLIENT_EXIT_CODE;
}

#define ECHO_INC    0
#define ECHO_END    1
typedef struct echo_msg_s {
    uint32_t cmd;
    uint32_t value;
} echo_msg_t;

static void test_noza_message()
{
    uint32_t code, server_vid, client_vid;
    TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&server_vid, string_server_thread, NULL, 0, 1024));
    TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&client_vid, string_client_thread, (void *)server_vid, 0, 1024));
    noza_thread_join(client_vid, &code);
    TEST_ASSERT_EQUAL_INT(CLIENT_EXIT_CODE, code);
    noza_thread_join(server_vid, &code);
    TEST_ASSERT_EQUAL_INT(SERVER_EXIT_CODE, code);
}

// test setjmp longjmp
#include <stdio.h>
#include <setjmp.h>

static jmp_buf env;

#define JUMP_VALUE  0x12345678
static void foo(void)
{
    longjmp(env, JUMP_VALUE);
}

static void test_noza_setjmp_longjmp()
{
    int ret = setjmp(env);
    if (ret == 0) {
        TEST_MESSAGE("initial call to setjmp");
        foo();
    } else {
        TEST_ASSERT_EQUAL_INT(JUMP_VALUE, ret);
    }
    TEST_MESSAGE("finish test");
}

// test hard fault
static int normal_task(void *param, uint32_t pid)
{
    int counter = 5;
    while (counter-->0) {
        noza_thread_sleep_ms(100, NULL);
    }
    //TEST_PRINTF("normal_task end. exit thread (%d)", pid);
    return pid;
}

static int fault_task(void *param, uint32_t pid)
{
    int counter = 3;
    while (counter-->0) {
        noza_thread_sleep_ms(100, NULL);
    }
    TEST_PRINTF("test raise fault (write memory address #00000000) pid=%d", pid);
    int *p = 0;
    *p = 0;
    TEST_ASSERT_EQUAL_INT(0, 1); // never reash here
}

static void test_noza_hardfault()
{
    uint32_t th, fid;
    TEST_MESSAGE("--- test hardfault ---");
    TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&th, normal_task, NULL, 0, 1024));
    TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&fid, fault_task, NULL, 0, 1024));
    uint32_t exit_code;
    TEST_ASSERT_EQUAL_INT(0, noza_thread_join(fid, &exit_code));
    TEST_ASSERT_EQUAL_INT(0, noza_thread_join(th, &exit_code));
    TEST_ASSERT_EQUAL_INT(th, exit_code);
}

#define ITERS 3000
static int counter = 0;
static int inc_task(void *param, uint32_t pid)
{
    mutex_t *mutex = (mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        TEST_ASSERT_EQUAL_INT(0, mutex_lock(mutex));
        counter++;
        TEST_ASSERT_EQUAL_INT(0, mutex_unlock(mutex));
    }
    return 0;
}

static int dec_task(void *param, uint32_t pid)
{
    mutex_t *mutex = (mutex_t *)param;
    for (int i=0; i<ITERS; i++) {
        TEST_ASSERT_EQUAL_INT(0, mutex_lock(mutex));
        counter--;
        TEST_ASSERT_EQUAL_INT(0, mutex_unlock(mutex));
    }
    return 0;
}

#define NUM_PAIR    4
static void test_noza_mutex()
{
    mutex_t noza_mutex;

    uint32_t inc_th[NUM_PAIR];
    uint32_t dec_th[NUM_PAIR];
    counter = 0; 
    TEST_ASSERT_EQUAL_INT(0, mutex_acquire(&noza_mutex));
    mutex_t *mutex = &noza_mutex;
    for (int i=0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&inc_th[i], inc_task, &noza_mutex, 1, 1024));
    }
    for (int i=0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&dec_th[i], dec_task, &noza_mutex, 1, 1024));
    }
    for (int i=0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_join(inc_th[i], NULL));
        TEST_ASSERT_EQUAL_INT(0, noza_thread_join(dec_th[i], NULL));
    }
    TEST_ASSERT_EQUAL_INT(0, counter);
    TEST_ASSERT_EQUAL_INT(0, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL_INT(EBUSY, mutex_trylock(&noza_mutex));
    TEST_ASSERT_EQUAL_INT(0, mutex_unlock(&noza_mutex));
    TEST_ASSERT_EQUAL_INT(0, mutex_release(&noza_mutex));
}

static void test_noza_lookup()
{
    // test name_lookup_server_register() and name_lookup_server_lookup()
    TEST_ASSERT_EQUAL_INT(0, name_lookup_register("key1", 1));
    TEST_ASSERT_EQUAL_INT(0, name_lookup_register("key2", 2));
    TEST_ASSERT_EQUAL_INT(0, name_lookup_register("key3", 3));

    uint32_t value;
    TEST_ASSERT_EQUAL_INT(0, name_lookup_search("key1", &value));
    TEST_ASSERT_EQUAL_INT(1, value);
    TEST_ASSERT_EQUAL_INT(0, name_lookup_search("key2", &value));
    TEST_ASSERT_EQUAL_INT(2, value);
    TEST_ASSERT_EQUAL_INT(0, name_lookup_search("key3", &value));
    TEST_ASSERT_EQUAL_INT(3, value);

    // test name_lookup_server_unregister()
    TEST_ASSERT_EQUAL_INT(0, name_lookup_unregister("key1"));
    TEST_ASSERT_EQUAL_INT(ENOENT, name_lookup_search("key1", &value)); // not found
    TEST_ASSERT_EQUAL_INT(0, name_lookup_search("key2", &value));
    TEST_ASSERT_EQUAL_INT(0, name_lookup_search("key3", &value));
    TEST_ASSERT_EQUAL_INT(0, name_lookup_unregister("key2"));
    TEST_ASSERT_EQUAL_INT(0, name_lookup_unregister("key3"));

    // test name_lookup_server_register() with too many services
    int i;
    for (i = 0; i < 128; i++) {
        char key[16];
        sprintf(key, "key%d", i);
        if (name_lookup_register(key, i) != 0)
            break;
    }
    int count = i;
    for (i = 0; i<count; i++) {
        char key[16];
        sprintf(key, "key%d", i);
        TEST_ASSERT_EQUAL_INT(0, name_lookup_unregister(key));
    }
    TEST_PRINTF("remaing service count: %d", count);
}

typedef struct spinlock_test_s {
    spinlock_t spinlock;
    int counter;
} spinlock_test_t;

#define PAIR_ITER 100000
static int spinlock_inc(void *param, uint32_t pid)
{
    spinlock_test_t *st = (spinlock_test_t *)param;
    int iter = PAIR_ITER;
    while (iter-->0) {
        noza_spinlock_lock(&st->spinlock);
        st->counter++;
        noza_spinlock_unlock(&st->spinlock);
    }
    return 0;
}

static int spinlock_dec(void *param, uint32_t pid)
{
    spinlock_test_t *st = (spinlock_test_t *)param;
    int iter = PAIR_ITER;
    while (iter-->0) {
        noza_spinlock_lock(&st->spinlock);
        st->counter--;
        noza_spinlock_unlock(&st->spinlock);
    }
    return 0;
}

static int test_lock_busy(void *param, uint32_t pid)
{
    TEST_ASSERT_EQUAL_INT(EBUSY, noza_spinlock_trylock((spinlock_t *)param));
    return 0;
}

static void test_noza_spinlock()
{
    uint32_t lth;
    spinlock_test_t spinlock_test;
    uint32_t inc_th[NUM_PAIR], dec_th[NUM_PAIR];

    memset(&spinlock_test, 0, sizeof(spinlock_test));
    TEST_ASSERT_EQUAL_INT(0, noza_spinlock_init(&spinlock_test.spinlock));
    TEST_ASSERT_EQUAL_INT(0, noza_spinlock_lock(&spinlock_test.spinlock));
    TEST_ASSERT_EQUAL_INT(EDEADLK, noza_spinlock_lock(&spinlock_test.spinlock));
    TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&lth, test_lock_busy, &spinlock_test.spinlock, 1, 1024));
    TEST_ASSERT_EQUAL_INT(0, noza_thread_join(lth, NULL));
    TEST_ASSERT_EQUAL_INT(0, noza_spinlock_unlock(&spinlock_test.spinlock));
    TEST_ASSERT_EQUAL_INT(0, noza_spinlock_free(&spinlock_test.spinlock));

    for (int i = 0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&inc_th[i], spinlock_inc, &spinlock_test.spinlock,
            (uint32_t)i%NOZA_OS_PRIORITY_LIMIT, 1024));
        TEST_ASSERT_NOT_EQUAL(0, inc_th[i]);
    }
    for (int i = 0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_create(&dec_th[i], spinlock_inc, &spinlock_test.spinlock,
            (uint32_t)i%NOZA_OS_PRIORITY_LIMIT, 1024));
        TEST_ASSERT_NOT_EQUAL(0, dec_th[i]);
    }

    for (int i = 0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_join(inc_th[i], NULL));
    }

    for (int i = 0; i < NUM_PAIR; i++) {
        TEST_ASSERT_EQUAL_INT(0, noza_thread_join(dec_th[i], NULL));
    }

    TEST_ASSERT_EQUAL_INT(0, counter);
}

static int test_all(int argc, char **argv)
{
    srand(time(0));
    UNITY_BEGIN();
    RUN_TEST(test_noza_setjmp_longjmp);
    RUN_TEST(test_thread_create_join);
    RUN_TEST(test_thread_sleep_and_signal);
    RUN_TEST(test_heavy_loading_thread);
    RUN_TEST(test_noza_thread_detach);
    RUN_TEST(test_thread_yield);
    RUN_TEST(test_futex_wait_wake);
    RUN_TEST(test_futex_timeout);
    RUN_TEST(test_timer_one_shot);
    RUN_TEST(test_timer_periodic);
    RUN_TEST(test_timer_cancel);
    RUN_TEST(test_clock_monotonic);
    RUN_TEST(test_signal_interrupts_futex);
    RUN_TEST(test_noza_message);
    RUN_TEST(test_noza_mutex);
    RUN_TEST(test_noza_hardfault);
    RUN_TEST(test_noza_lookup);
    RUN_TEST(test_noza_spinlock);
    UNITY_END();
    return 0;
}

#include "user/console/noza_console.h"

static int futex_only(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_futex_wait_wake);
    RUN_TEST(test_futex_timeout);
    UNITY_END();
    return 0;
}

#include "user/console/noza_console.h"
void __attribute__((constructor(1000))) register_noza_unittest()
{
    console_add_command("noza_unittest", test_all, "nozaos and lib, unit-test suite", 2048);
}

void __attribute__((constructor(1001))) register_futex_command()
{
    console_add_command("futex_test", futex_only, "run futex-only unit tests", 2048);
}
