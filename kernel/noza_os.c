#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "hardware/structs/systick.h"
#include "hardware/sync.h"
#include "noza_config.h"
#include "syscall.h"

#define THREAD_FREE             0
#define THREAD_RUNNING          1
#define THREAD_READY            2
#define THREAD_WAIT_IO          3
#define THREAD_SLEEP            4
#define THREAD_HARDFAULT        6

#define SYSCALL_DONE            0
#define SYSCALL_PENDING         1
#define SYSCALL_SERVING         2

extern void init_task(void *param);

typedef struct cdl_node_s cdl_node_t;
struct cdl_node_s {
    cdl_node_t *next;
    cdl_node_t *prev;
    void *value;
};

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3; // the pointer of return value
    uint32_t state;
} kernel_trap_info_t;

typedef struct {
    void                (*entry)(void *param);
    void                *param;
    uint32_t            *stack_ptr;
    cdl_node_t          state_node;
    uint32_t            priority;
    uint32_t            state;
    uint32_t            pid;
    uint32_t            expired_time;
    kernel_trap_info_t  trap;
    uint32_t            stack_area[NOZA_OS_STACK_SIZE];
} thread_t;

typedef struct {
    cdl_node_t  *head;
    uint32_t    count;
    uint32_t    state_id;
} thread_list_t;

typedef struct {
    thread_t        *running[NOZA_OS_NUM_CORES];
    thread_list_t   ready[NOZA_OS_PRIORITY_LIMIT];
    thread_list_t   wait;
    thread_list_t   sleep;
    thread_list_t   hardfault;
    thread_list_t   free;
    #if NOZA_OS_NUMCORE > 1
    mutex_t         mutex;
    #endif

    // context
    thread_t thread[NOZA_OS_TASK_LIMIT];
} noza_os_t;

// the kernel data structure
static noza_os_t noza_os;

static void     noza_os_add_thread(thread_list_t *list, thread_t *thread);
static void     noza_os_remove_thread(thread_list_t *list, thread_t *thread);
static uint32_t noza_os_thread_create( void (*entry)(void *param), void *param, uint32_t pri);
static void     noza_os_scheduler();

#if NOZA_OS_NUM_CORES > 1
#define noza_os_enter_blocking(&noza_os.mutex) mutex_enter_blocking(&noza_os.mutex)
#define noza_os_mutex_exit(&noza_os.mutex) mutex_exit(&noza_os.mutex)
#else
#define noza_os_enter_blocking(m)  (void)0  
#define noza_os_mutex_exit(m) (void)0
#endif

void noza_start()
{
    int i;
    stdio_init_all();

    // setup the interrupt priority
    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR2_OFFSET), M0PLUS_SHPR2_BITS); // setup as priority 3
    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR3_OFFSET), M0PLUS_SHPR3_BITS); // setup as priority 3

    // init noza os / thread structure
    memset(&noza_os, 0, sizeof(noza_os_t));
    #if NOZA_OS_NUMCORE > 1
    mutex_init(&noza_os.mutex);
    #endif
    for (i=0; i<NOZA_OS_PRIORITY_LIMIT; i++) {
        noza_os.ready[i].state_id = THREAD_READY;
    }
    noza_os.wait.state_id = THREAD_WAIT_IO;
    noza_os.sleep.state_id = THREAD_SLEEP;
    noza_os.free.state_id = THREAD_FREE;

    for (i=0; i<NOZA_OS_TASK_LIMIT; i++) {
        noza_os.thread[i].state_node.value = &noza_os.thread[i];
        noza_os.thread[i].pid = i;
        noza_os_add_thread(&noza_os.free, &noza_os.thread[i]);
    }

    // create application thread
    noza_os_thread_create(init_task, NULL, 0);

    noza_os_scheduler();
}

static thread_t *noza_thread_alloc()
{
    if (noza_os.free.count == 0) {
        panic("noza_thread_alloc: no free thread\n");
        return NULL;
    }

    thread_t *th = (thread_t *)noza_os.free.head->value;
    noza_os_remove_thread(&noza_os.free, th);

    return th;
}

static cdl_node_t *cdl_add(cdl_node_t *head, cdl_node_t *obj)
{
    if (head == NULL) {
        obj->next = obj;
        obj->prev = obj;
        return obj;
    } else if (head->next == head) {
        obj->next = head;
        obj->prev = head;
        head->next = obj;
        head->prev = obj;
        return head;
    } else {
        obj->next = head;
        obj->prev = head->prev;
        head->prev->next = obj;
        head->prev = obj;
        return head;
    }
}

static cdl_node_t *cdl_remove(cdl_node_t *head, cdl_node_t *obj)
{
    if (head == obj) {
        if (head->next == head) {
            return NULL;
        } else {
            if (obj->next == head && obj->prev == head) {
                cdl_node_t *new_head = head->next;
                new_head->prev = new_head->next = new_head;
                return new_head;
            } else {
                cdl_node_t *new_head = head->next;
                new_head->prev = head->prev;
                head->prev->next = new_head;
                return new_head;
            }
        }
    }
    if (obj->next == head && obj->prev == head) {
        head->next = head->prev = head;
        return head;
    }
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    return head;
}

static void noza_os_add_thread(thread_list_t *list, thread_t *thread)
{
    list->head = cdl_add(list->head, &thread->state_node);
    list->count++;
    thread->state = list->state_id;
}

static void noza_os_remove_thread(thread_list_t *list, thread_t *thread)
{
    list->head = cdl_remove(list->head, &thread->state_node);
    list->count--;
}

static void bootstrap()
{
    __asm__ (
        "blx r3" // keep r0, r1, r2 as the parameters and jump to r3
    );

    // terminate the thread
    extern uint32_t noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
    noza_syscall(NSC_TERMINATE_THREAD, 0, 0, 0);
}

static void noza_make_context(thread_t *th, void *param)
{
    // This task_stack frame needs to mimic would be saved by hardware and by the software (in isr_svcall)
    /*
        Exception frame saved by the hardware onto task_stack:
        +------+
        | xPSR | task_stack[16] 0x01000000 i.e. PSR thumb bit
        |  PC  | task_stack[15] pointer_to_task_function
        |  LR  | task_stack[14]
        |  R12 | task_stack[13]
        |  R3  | task_stack[12]
        |  R2  | task_stack[11]
        |  R1  | task_stack[10]
        |  R0  | task_stack[9]
        +------+
        Registers saved by the software (isr_svcall):
        +------+
        |  LR  | task_stack[8]	(THREAD_PSP i.e. 0xFFFFFFFD)
        |  R7  | task_stack[7]
        |  R6  | task_stack[6]
        |  R5  | task_stack[5]
        |  R4  | task_stack[4]
        |  R11 | task_stack[3]
        |  R10 | task_stack[2]
        |  R9  | task_stack[1]
        |  R8  | task_stack[0]
        +------+
    */

    // setup thread boot address in stack
    th->stack_ptr = th->stack_area + NOZA_OS_STACK_SIZE - 17; // end of task_stack, minus what we are about to push
    th->stack_ptr[8] = (uint32_t) NOZA_OS_THREAD_PSP;
    th->stack_ptr[15] = (uint32_t) bootstrap;
    th->stack_ptr[16] = (uint32_t) 0x01000000; // PSR thumb bit

    // setup context for thread
    th->stack_ptr[9] = (uint32_t) param;
    th->stack_ptr[10] = (uint32_t) th->pid;
    th->stack_ptr[11] = 0x00; // reserved
    th->stack_ptr[12] = (uint32_t) th->entry;
}

static uint32_t noza_os_thread_create( void (*entry)(void *param), void *param, uint32_t pri)
{
    noza_os_enter_blocking(&noza_os.mutex);
    thread_t *th = noza_thread_alloc();
    th->entry = entry;
    th->param = param;
    th->priority = pri;
    noza_os_add_thread(&noza_os.ready[pri], th);
    noza_os_mutex_exit(&noza_os.mutex);
    noza_make_context(th, param);

    return th->pid;
}

static void noza_systick_config(unsigned int n)
{
    // stop systick and cancel it if it is pending
    systick_hw->csr = 0;    // disable timer and IRQ 
    __dsb();                // make sure systick is disabled
    __isb();                // and it is really off

    // clear the systick exception pending bit if it got set
    hw_set_bits  ((io_rw_32 *)(PPB_BASE + M0PLUS_ICSR_OFFSET),M0PLUS_ICSR_PENDSTCLR_BITS);

    systick_hw->rvr = (n) - 1UL;    // set the reload value
    systick_hw->cvr = 0;    // clear counter to force reload
    systick_hw->csr = 0x03; // arm IRQ, start counter with 1 usec clock
}

void init_kernel_stack(uint32_t *stack); // assembly
static void noza_switch_handler(void)
{
    uint32_t dummy[32];
    init_kernel_stack(dummy+32);
}

static void serv_syscall(uint32_t core) 
{
    thread_t *th = noza_os.running[core];
    uint32_t *ret_addr = (uint32_t *)th->trap.r3;
    th->trap.state = SYSCALL_SERVING;
    switch (th->trap.r0) {
        case NSC_SLEEP:
            th->expired_time = to_ms_since_boot(get_absolute_time()) + th->trap.r1; // setup expired time
            noza_os_enter_blocking(&noza_os.mutex);
            noza_os_add_thread(&noza_os.sleep, th);
            noza_os_mutex_exit(&noza_os.mutex);
            noza_os.running[core] = NULL;
            break;

        case NSC_CREATE_THREAD:
            *ret_addr = noza_os_thread_create((void (*)(void *))th->trap.r1, (void *)th->trap.r2, 0);
            break;

        case NSC_TERMINATE_THREAD:
            noza_os_enter_blocking(&noza_os.mutex);
            noza_os_add_thread(&noza_os.free, th);
            noza_os.running[core] = NULL;
            noza_os_mutex_exit(&noza_os.mutex);
            th->trap.state = SYSCALL_DONE;
            break;

        case NSC_HARDFAULT:
            noza_os_enter_blocking(&noza_os.mutex);
            noza_os_add_thread(&noza_os.hardfault, th);
            noza_os_mutex_exit(&noza_os.mutex);
            noza_os.running[core] = NULL;
            th->trap.state = SYSCALL_DONE;
            break;

        default:
            // syscall code not found
            th->trap.state = SYSCALL_DONE;
            return;
    }
}

static uint32_t noza_check_sleep_thread(uint32_t slice)
{
    #define EXPIRED_LIMIT   4
    uint32_t counter = 0;
    thread_t *expired_thread[EXPIRED_LIMIT];

    if (noza_os.sleep.count == 0)
        return slice;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    for (;;) {
        cdl_node_t *cursor = noza_os.sleep.head;
        for (int i=0; i<noza_os.sleep.count; i++) {
            thread_t *th = (thread_t *)cursor->value;
            if (th->expired_time <= now) {
                expired_thread[counter++] = th;
                if (counter >= EXPIRED_LIMIT) {
                    break;
                }
            } else {
                uint32_t diff = th->expired_time - now;
                if (diff < slice)
                    slice = diff;
            }
            cursor = cursor->next;
        }

        if (counter>0) {
            for (int i=0; i<counter; i++) {
                thread_t *th = expired_thread[i];
                noza_os_remove_thread(&noza_os.sleep, th);
                noza_os_add_thread(&noza_os.ready[th->priority], th);
                *((uint32_t *)th->trap.r3) = 0; // return value
            }
            counter = 0;
        } else
            break;
    }
}

uint32_t *noza_os_resume_thread(uint32_t *stack);
static void noza_os_scheduler()
{
    uint32_t core = get_core_num();
    noza_switch_handler();

    for (;;) {
        noza_os_enter_blocking(&noza_os.mutex);
        for (int i=0; i<NOZA_OS_PRIORITY_LIMIT; i++) {
            if (noza_os.ready[i].count > 0) {
                noza_os.running[core] = noza_os.ready[i].head->value;
                noza_os_remove_thread(&noza_os.ready[i], noza_os.running[core]);
                break;
            }
        }
        noza_os_mutex_exit(&noza_os.mutex);
        if (noza_os.running[core]) {
            noza_os.running[core]->state = THREAD_RUNNING;
            for (;;) {
                noza_os.running[core]->stack_ptr = noza_os_resume_thread(noza_os.running[core]->stack_ptr);
                if (noza_os.running[core]->trap.state == SYSCALL_PENDING) {
                    serv_syscall(core);
                    if (noza_os.running[core] == NULL)
                        break;
                }
            }
            if (noza_os.running[core] != NULL) {
                noza_os_enter_blocking(&noza_os.mutex);
                noza_os_add_thread(&noza_os.ready[noza_os.running[core]->priority], noza_os.running[core]);
                noza_os_mutex_exit(&noza_os.mutex);
                noza_os.running[core] = NULL;
            }
        } else {
            __wfi(); // no task, wait for interrupt
        }
        noza_systick_config(noza_check_sleep_thread(NOZA_OS_TIME_SLICE));
    }
}

// software interrupt, system call
// called from assembly SVC0 handler
void noza_os_trap_info(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    kernel_trap_info_t *trap = &noza_os.running[get_core_num()]->trap;
    trap->r0 = r0;
    trap->r1 = r1;
    trap->r2 = r2;
    trap->r3 = r3;
    trap->state = SYSCALL_PENDING;
}

void main()
{
    noza_start();
}
