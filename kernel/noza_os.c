#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "hardware/structs/systick.h"
#include "hardware/sync.h"
#include "noza_config.h"
#include "syscall.h"

//////////////////////////////////////////////////////////////
//
// Noza Kernel data structures
//
#define THREAD_FREE             0
#define THREAD_RUNNING          1
#define THREAD_READY            2
#define THREAD_WAITING          3
#define THREAD_SLEEP            4
#define THREAD_HARDFAULT        6

#define SYSCALL_DONE            0
#define SYSCALL_PENDING         1
#define SYSCALL_SERVING         2
#define SYSCALL_OUTPUT          3 

#define TYPE_NONE_SYSCALL       0
#define TYPE_SYSCALL            1

#define PORT_WAIT_LISTEN        0
#define PORT_READY              1

extern void __user_start(void *param); // the first user task

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
    uint32_t r3;
    uint32_t type;
    uint32_t state;
} kernel_trap_info_t;

typedef struct {
    cdl_node_t  *head;
    uint32_t    count;
    uint32_t    state_id;
} thread_list_t;

typedef struct thread_s thread_t;
typedef struct {
    thread_list_t   reply_list;     // a list of threads waiting for reply
    thread_list_t   pending_list;   // a list of threads waiting for this port
    thread_t        *join_thread;   // the thread which is processing the message of the port
} noza_os_port_t;

typedef struct {
    uint32_t    priority:4;
    uint32_t    state:4;
    uint32_t    port_state:1;       // PORT_WAIT_LISTEN or PORT_READY
} info_t;

typedef struct thread_s {
    uint32_t            *stack_ptr;
    cdl_node_t          state_node;
    info_t              info;
    uint32_t            expired_time;
    kernel_trap_info_t  trap;
    noza_os_port_t      port;
    uint32_t            msg;
    uint32_t            stack_area[NOZA_OS_STACK_SIZE];
} thread_t;


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


/////////////////////////////////////////////////////////////////////////////////////////
//
// declare the Noza kernel data structure instance
//
static noza_os_t noza_os;
inline static uint32_t thread_get_pid(thread_t *th)
{
    return (th - (thread_t *)&noza_os.thread[0]);
}

static void     noza_os_add_thread(thread_list_t *list, thread_t *thread);
static void     noza_os_remove_thread(thread_list_t *list, thread_t *thread);
static uint32_t noza_os_thread_create( void (*entry)(void *param), void *param, uint32_t pri);
static void     noza_os_scheduler();

#if NOZA_OS_NUM_CORES > 1
#define noza_os_enter_blocking(&noza_os.mutex) mutex_enter_blocking(&noza_os.mutex)
#define noza_os_mutex_exit(&noza_os.mutex) mutex_exit(&noza_os.mutex)
#else
//#define noza_os_enter_blocking(m)  (void)0  
//#define noza_os_mutex_exit(m) (void)0
#endif

inline static void noza_os_set_return_value(thread_t *th, uint32_t ret)
{
    th->trap.r0 = ret;
    th->trap.state = SYSCALL_OUTPUT;
}

inline static void noza_os_change_state(thread_t *th, thread_list_t *from, thread_list_t *to)
{
    noza_os_remove_thread(from, th);
    noza_os_add_thread(to, th);
}

inline static thread_t *noza_os_get_running_thread()
{
    return noza_os.running[get_core_num()];
}

inline static void noza_os_clear_running_thread()
{
    noza_os.running[get_core_num()] = NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////
// 
// Noza IPC 
// 
static void noza_os_send(thread_t *running, thread_t *target, uint32_t msg)
{
    running->msg = msg; // save message
    //switch (target->port.state) {
    switch (target->info.port_state) {
        case PORT_READY: // if the target port is ready, then the target thread is blocked in the wait list
            // change the target thread state from wait list to ready list
            noza_os_change_state(target, &noza_os.wait, &noza_os.ready[target->info.priority]); 
            noza_os_change_state(running, &noza_os.ready[running->info.priority], &target->port.reply_list);
            target->port.join_thread = running;
            target->info.port_state = PORT_WAIT_LISTEN; 
            break;

        case PORT_WAIT_LISTEN:
            noza_os_change_state(running, &noza_os.ready[running->info.priority], &target->port.pending_list);
            break;
    }
    noza_os_clear_running_thread(); // clear the running thread
}

static void noza_os_nonblock_send(thread_t *running, thread_t *target, uint32_t msg)
{
    if (target->info.port_state == PORT_READY) {
        noza_os_send(running, target, msg);
    } else {
        noza_os_set_return_value(running, -1);
    }
}

static void noza_os_recv(thread_t *running)
{
    if (running->port.pending_list.count > 0) {
        thread_t *source = running->port.pending_list.head->value;
        noza_os_set_return_value(running, (uint32_t)source->msg);
        noza_os_set_return_value(source, 0); // send success
        noza_os_change_state(source, &running->port.pending_list, &running->port.reply_list);
        running->port.join_thread = source;
        noza_os_set_return_value(running, 0);
    } else {
        noza_os_add_thread(&noza_os.wait, running); // add work thread into wait list
        noza_os_clear_running_thread();
        running->info.port_state = PORT_READY; 
    }
}

static void noza_os_nonblock_recv(thread_t *running)
{
    if (running->port.pending_list.count > 0) {
        noza_os_recv(running);
    } else {
        noza_os_set_return_value(running, -1);
    }
}

static void noza_os_reply(thread_t *running, uint32_t msg)
{
    thread_t *join_thread = running->port.join_thread;
    if (join_thread) {
        noza_os_change_state(join_thread, &running->port.reply_list, &noza_os.ready[join_thread->info.priority]);
        noza_os_set_return_value(join_thread, msg);
        noza_os_set_return_value(running, 0); // success
        running->info.port_state = PORT_WAIT_LISTEN; // change state to wait listen
        running->port.join_thread = NULL; // clear
    }
}

// start the noza os
static void noza_start()
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
    noza_os.wait.state_id = THREAD_WAITING;
    noza_os.sleep.state_id = THREAD_SLEEP;
    noza_os.free.state_id = THREAD_FREE;

    for (i=0; i<NOZA_OS_TASK_LIMIT; i++) {
        noza_os.thread[i].state_node.value = &noza_os.thread[i];
        noza_os_add_thread(&noza_os.free, &noza_os.thread[i]);
        noza_os.thread[i].port.pending_list.state_id = THREAD_WAITING;
        noza_os.thread[i].port.reply_list.state_id = THREAD_WAITING;
    }

    // create application thread
    noza_os_thread_create(__user_start, NULL, 0);

    // run scheduler
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

static void noza_thread_clear(thread_t *th)
{
    while (th->port.pending_list.count>0) {
        thread_t *pending = (thread_t *)th->port.pending_list.head->value;
        noza_os_set_return_value(pending, -1);
        noza_os_change_state(pending, &th->port.pending_list, &noza_os.ready[pending->info.priority]);
    }
    while (th->port.reply_list.count>0) {
        thread_t *reply = (thread_t *)th->port.reply_list.head->value;
        noza_os_set_return_value(reply, -1);
        noza_os_change_state(reply, &th->port.reply_list, &noza_os.ready[reply->info.priority]);
    }
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
    thread->info.state = list->state_id;
}

static void noza_os_remove_thread(thread_list_t *list, thread_t *thread)
{
    list->head = cdl_remove(list->head, &thread->state_node);
    list->count--;
}

/*
static void bootstrap() // TODO: bootstrap is in user mode, move to libsys
{
    __asm__ (
        "blx r3" // keep r0, r1, r2 as the parameters and jump to r3
    );

    // terminate the thread
    extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
    noza_syscall(NSC_TERMINATE_THREAD, 0, 0, 0); 
}
*/

typedef struct {
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t lr;
} user_stack_t;

// registered saved by hardware, not by software
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12; // r12 is not saved by hardware, but by software (isr_svcall)
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr; // psr thumb bit
} interrupted_stack_t;

static void noza_make_context(thread_t *th, void (*entry)(void *param), void *param)
{
    th->stack_ptr = th->stack_area + NOZA_OS_STACK_SIZE - 17; // end of task_stack, minus what we are about to push

    user_stack_t *u = (user_stack_t *) th->stack_ptr;
    u->lr = (uint32_t) NOZA_OS_THREAD_PSP; // return to thread mode, use PSP

    interrupted_stack_t *is = (interrupted_stack_t *)(th->stack_ptr + (sizeof(user_stack_t)/4));
    //is->pc = (uint32_t) bootstrap;
    is->pc = (uint32_t) entry;
    is->xpsr = (uint32_t) 0x01000000; // thumb bit

    // pass parameters to bootstrap
    is->r0 = (uint32_t) param;
    //is->r1 = (uint32_t) thread_get_pid(th);
    //is->r2 = (uint32_t) 0x00; // reserved
    //is->r3 = (uint32_t) entry; // TODO: bootstrap should be in libc, change this later
}

static uint32_t noza_os_thread_create( void (*entry)(void *param), void *param, uint32_t pri)
{
    thread_t *th = noza_thread_alloc();
    th->info.priority = pri;
    noza_os_add_thread(&noza_os.ready[pri], th);
    noza_make_context(th, entry, param);

    return thread_get_pid(th);
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

//////////////////////////////////////////////////////////////
//
// system call implementation
//
void init_kernel_stack(uint32_t *stack); // assembly
static void noza_switch_handler(void)
{
    uint32_t dummy[32];
    init_kernel_stack(dummy+32);
}

static void syscall_yield(thread_t *running)
{
    noza_os_set_return_value(running, 0);
    noza_os_clear_running_thread(); // force select the next thread
    noza_os_add_thread(&noza_os.ready[running->info.priority], running); // insert the thread back to ready queue
}

static void syscall_sleep(thread_t *running)
{
    running->expired_time = to_ms_since_boot(get_absolute_time()) + running->trap.r1; // setup expired time
    noza_os_add_thread(&noza_os.sleep, running);
    noza_os_clear_running_thread();
}

static void syscall_thread_create(thread_t *running)
{
    uint32_t ret_value = noza_os_thread_create((void (*)(void *))running->trap.r1, (void *)running->trap.r2, running->trap.r3); 
    noza_os_set_return_value(running, ret_value);
}

static void syscall_thread_change_priority(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    thread_t *target = &noza_os.thread[running_trap->r1]; // TODO: introduce the capability and do sanity check
    if (target == running) {
        running->info.priority = running->trap.r2;
    } else if (target->info.state != THREAD_READY) {
        target->info.priority = running_trap->r2;
    } else {
        // target is already in ready list
        noza_os_remove_thread(&noza_os.ready[target->info.priority], target);
        target->info.priority = running_trap->r2;
        noza_os_add_thread(&noza_os.ready[target->info.priority], target);
    }
}

static void syscall_thread_terminate(thread_t *running)
{
    noza_os_add_thread(&noza_os.free, running);
    noza_os_clear_running_thread();
    noza_thread_clear(running); // TODO: rename this
    running->trap.state = SYSCALL_DONE; // thread is terminated, it is fine to remove this line
}

static void syscall_recv(thread_t *running)
{
    noza_os_recv(running);
}

static void syscall_reply(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    noza_os_reply(running, running_trap->r1);
}

static void syscall_call(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    thread_t *target = &noza_os.thread[running_trap->r1]; // TODO: introduce the capability and do sanity check
    noza_os_send(running, target, running_trap->r2);
    noza_os_recv(running);
}

static void syscall_nbcall(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    thread_t *target = &noza_os.thread[running_trap->r1]; // TODO: introduce the capability and do sanity check
    noza_os_nonblock_send(running, target, running_trap->r1);
}

static void syscall_nbrecv(thread_t *running)
{
    noza_os_nonblock_recv(running);
}

typedef void (*syscall_func_t)(thread_t *source);

static syscall_func_t syscall_func[] = {
    [NSC_YIELD] = syscall_yield,
    [NSC_SLEEP] = syscall_sleep,
    [NSC_THREAD_CREATE] = syscall_thread_create,
    [NSC_THREAD_CHANGE_PRIORITY] = syscall_thread_change_priority,
    [NSC_THREAD_TERMINATE] = syscall_thread_terminate,

    [NSC_RECV] = syscall_recv,
    [NSC_REPLY] = syscall_reply,
    [NSC_CALL] = syscall_call,
    [NSC_NB_RECV] = syscall_nbrecv,
    [NSC_NB_CALL] = syscall_nbcall,
};

static void serv_syscall(uint32_t core)
{
    thread_t *source = noza_os_get_running_thread();
    if (source->trap.r0 >= 0 && source->trap.r0 < NSC_NUM_SYSCALLS && syscall_func[source->trap.r0]) {
        syscall_func_t syscall = syscall_func[source->trap.r0];
        source->trap.state = SYSCALL_SERVING;
        syscall(source);
    } else if (source->trap.r0 == NSC_HARDFAULT) {
        syscall_thread_terminate(source);
        source->trap.state = SYSCALL_DONE;
    } else {
        // unknown of systick
        source->trap.state = SYSCALL_DONE;
    }
}
//////////////////////////////////////////////////////////////

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
                noza_os_change_state(th, &noza_os.sleep, &noza_os.ready[th->info.priority]);
                noza_os_set_return_value(th, 0); // return value
            }
            counter = 0;
        } else
            break;
    }

    return 0;
}

uint32_t *noza_os_resume_thread(uint32_t *stack);
static void noza_os_scheduler()
{
    uint32_t core = get_core_num();
    noza_switch_handler();

    for (;;) {
        thread_t *running = NULL;
        for (int i=0; i<NOZA_OS_PRIORITY_LIMIT; i++) {
            if (noza_os.ready[i].count > 0) {
                running = noza_os.ready[i].head->value;
                noza_os_remove_thread(&noza_os.ready[i], running);
                break;
            }
        }
        if (running) { // a ready thread is picked
            running->info.state = THREAD_RUNNING;
            for (;;) {
                noza_os.running[core]= running;
                if (running->trap.state == SYSCALL_OUTPUT) {
                    interrupted_stack_t *is = (interrupted_stack_t *)(running->stack_ptr + (sizeof(user_stack_t)/sizeof(uint32_t)));
                    is->r0 = running->trap.r0;
                    is->r1 = running->trap.r1;
                    is->r2 = running->trap.r2;
                    is->r3 = running->trap.r3;
                    running->trap.state = SYSCALL_DONE;
                }
                running->stack_ptr = noza_os_resume_thread(running->stack_ptr);
                if (running->trap.state == SYSCALL_PENDING) {
                    serv_syscall(core);
                    if (noza_os_get_running_thread() == NULL)
                        break;
                }
            }
            if (noza_os.running[core] != NULL) {
                noza_os_add_thread(&noza_os.ready[noza_os.running[core]->info.priority], noza_os.running[core]);
                noza_os_clear_running_thread();
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
    // callback from SVC servie routine, make system call pending
    // and trap into kernel later
    kernel_trap_info_t *trap = &(noza_os_get_running_thread()->trap);
    trap->r0 = r0;
    trap->r1 = r1;
    trap->r2 = r2;
    trap->r3 = r3;
    trap->type = (r0 < NSC_NUM_SYSCALLS) ? TYPE_SYSCALL : TYPE_NONE_SYSCALL;
    trap->state = SYSCALL_PENDING;
}

int main()
{
    noza_start();
    return 0;
}
