#include <string.h>
#include <stdio.h>

#include "noza_config.h"
#include "syscall.h"
#include "platform.h"

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

#define PORT_WAIT_LISTEN        0
#define PORT_READY              1

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
} noza_os_port_t;

typedef struct {
    uint32_t    priority:4;
    uint32_t    state:4;
    uint32_t    port_state:1;       // PORT_WAIT_LISTEN or PORT_READY
} info_t;

typedef struct {
    union {
        uint32_t   target;
        uint32_t   reply;
    } pid;
    uint32_t   size;
    void       *ptr;
} noza_os_message_t;

// define a structure for thread management
typedef struct thread_s {
    uint32_t            *stack_ptr;          // pointer to the thread's stack
    cdl_node_t          state_node;          // node for managing the thread state
    info_t              info;                // thread information
    uint32_t            expired_time;        // time when the thread expires
    kernel_trap_info_t  trap;                // kernel trap information
    noza_os_port_t      port;                // port information
    noza_os_message_t   message;             // message passing mechanism for the thread
    thread_list_t       join_list;           // list of threads waiting to join this thread
    uint32_t            stack_area[NOZA_OS_STACK_SIZE]; // stack memory area for the thread
} thread_t;

// Define a structure for Noza OS management
typedef struct {
    thread_t        *running[NOZA_OS_NUM_CORES];         // array of currently running threads for each core
    thread_list_t   ready[NOZA_OS_PRIORITY_LIMIT];       // array of ready threads for each priority level
    thread_list_t   wait;                                // list of threads in waiting state
    thread_list_t   sleep;                               // list of threads in sleeping state
    thread_list_t   hardfault;                           // list of threads in hardfault state
    thread_list_t   free;                                // list of free/available threads
    // context
    thread_t thread[NOZA_OS_TASK_LIMIT];                 // array of thread_t structures for task management
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
#define noza_os_lock_init()     platform_os_lock_init()
#define noza_os_lock(core)      platform_os_lock(core)
#define noza_os_unlock(core)    platform_os_unlock(core)
#else
#define noza_os_lock_init()     (void)0
#define noza_os_lock(core)      (void)0
#define noza_os_unlock(core)    (void)0
#endif

inline static void noza_os_set_return_value(thread_t *th, uint32_t r0)
{
    th->trap.r0 = r0;
    th->trap.state = SYSCALL_OUTPUT;
}

inline static void noza_os_set_return_value2(thread_t *th, uint32_t r0, uint32_t r1)
{
    th->trap.r0 = r0;
    th->trap.r1 = r1;
    th->trap.state = SYSCALL_OUTPUT;
}

inline static void noza_os_set_return_value3(thread_t *th, uint32_t r0, uint32_t r1, uint32_t r2)
{
    th->trap.r0 = r0;
    th->trap.r1 = r1;
    th->trap.r2 = r2;
    th->trap.state = SYSCALL_OUTPUT;
}

inline static void noza_os_set_return_value4(thread_t *th, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    th->trap.r0 = r0;
    th->trap.r1 = r1;
    th->trap.r2 = r2;
    th->trap.r3 = r3;
    th->trap.state = SYSCALL_OUTPUT;
}

inline static void noza_os_change_state(thread_t *th, thread_list_t *from, thread_list_t *to)
{
    noza_os_remove_thread(from, th);
    noza_os_add_thread(to, th);
}

inline static thread_t *noza_os_get_running_thread()
{
    return noza_os.running[platform_get_running_core()];
}

inline static void noza_os_set_running_thread(thread_t *th)
{
    noza_os.running[platform_get_running_core()] = th;
}

inline static void noza_os_clear_running_thread()
{
    noza_os.running[platform_get_running_core()] = NULL;
}


/////////////////////////////////////////////////////////////////////////////////////////
// 
// Noza IPC 
// 
static void noza_os_send(thread_t *running, thread_t *target, void *msg, uint32_t size)
{
    running->message.pid.target = thread_get_pid(target); 
    running->message.ptr = msg;
    running->message.size = size;
    switch (target->info.port_state) {
        case PORT_READY:
            // if the target port is ready, then the target thread is blocked in the wait list
            // change the target thread state from wait list to ready list
            // because the running thread is not in the ready list, just add it to reply list
            noza_os_add_thread(&target->port.reply_list, running);
            // target thread should be in the wait list already, because the port state is PORT_READY
            // so just change the state from wait list to ready list
            noza_os_change_state(target, &noza_os.wait, &noza_os.ready[target->info.priority]); // TODO: BUG....what if target is not on wait list?
            noza_os_set_return_value4(target, 0, thread_get_pid(running), (uint32_t) msg, size); // 0 --> success
            target->info.port_state = PORT_WAIT_LISTEN; 
            break;

        case PORT_WAIT_LISTEN:
            // because the running state is already not on the ready list
            // just add it into pending list 
            noza_os_add_thread(&target->port.pending_list, running);
            break;
    }
    noza_os_clear_running_thread(); // clear the running thread
}

static void noza_os_nonblock_send(thread_t *running, thread_t *target, void *msg, uint32_t size)
{
    if (target->info.port_state == PORT_READY) {
        noza_os_send(running, target, msg, size);
    } else {
        noza_os_set_return_value(running, -1);
    }
}

inline static uint32_t noza_os_get_ready_count()
{
    uint32_t count = 0;
    for (int i = 0; i < NOZA_OS_PRIORITY_LIMIT; i++) {
        count += noza_os.ready[i].count;
    }
    return count;
}

static void noza_os_recv(thread_t *running)
{
    if (running->port.pending_list.count > 0) {
        thread_t *source = running->port.pending_list.head->value;
        noza_os_set_return_value4(running, 0, thread_get_pid(source), (uint32_t)source->message.ptr, source->message.size);
        noza_os_set_return_value(source, 0); // send success
        noza_os_change_state(source, &running->port.pending_list, &running->port.reply_list);
        noza_os_set_return_value(running, 0);
    } else {
        running->info.port_state = PORT_READY; 
        // because running is already removed from ready list, so just add it to wait list
        noza_os_add_thread(&noza_os.wait, running);
        noza_os_clear_running_thread();
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

static void noza_os_reply(thread_t *running, uint32_t pid, void *msg, uint32_t size)
{
    // search if pid is in the reply list (sanity check)
    thread_t *head_th = (thread_t *)running->port.reply_list.head->value;
    thread_t *th = head_th;
    while (th) {
        if (thread_get_pid(th) == pid) {
            break;
        }
        th = th->state_node.next->value;
        if (th == head_th) { // not found in reply list !
            noza_os_set_return_value(running, -1); // error
            return;
        }
    }

    noza_os_change_state(th, &running->port.reply_list, &noza_os.ready[th->info.priority]);
    noza_os_set_return_value3(th, 0, (uint32_t)msg, size); // 0 -> send/reply success, TODO: think about if error code is needed
    noza_os_set_return_value(running, 0); // success
    running->info.port_state = PORT_WAIT_LISTEN; // change state to wait listen
}

static void noza_os_join(thread_t *running, uint32_t pid)
{
    if (pid >= NOZA_OS_TASK_LIMIT) {
        noza_os_set_return_value(running, -1); // error
        return;
    }
    thread_t *th = &noza_os.thread[pid];
    if (th->info.state == THREAD_FREE) {
        noza_os_set_return_value(running, -1); // error
        return;
    }

    noza_os_add_thread(&th->join_list, running);
    noza_os_clear_running_thread();
}

static void noza_make_idle_context(uint32_t core);

// start the noza os
static void noza_init()
{
    int i;
    platform_io_init(); // initial all state in platform

    // init noza os / thread structure
    memset(&noza_os, 0, sizeof(noza_os_t));
    noza_os_lock_init();
    for (i=0; i<NOZA_OS_PRIORITY_LIMIT; i++) {
        noza_os.ready[i].state_id = THREAD_READY;
    }
    noza_os.wait.state_id = THREAD_WAITING;
    noza_os.sleep.state_id = THREAD_SLEEP;
    noza_os.free.state_id = THREAD_FREE;
    noza_os.hardfault.state_id = THREAD_HARDFAULT;

    // init all thread structure
    for (i=0; i<NOZA_OS_TASK_LIMIT; i++) {
        noza_os.thread[i].state_node.value = &noza_os.thread[i];
        noza_os_add_thread(&noza_os.free, &noza_os.thread[i]);
        noza_os.thread[i].port.pending_list.state_id = THREAD_WAITING;
        noza_os.thread[i].port.reply_list.state_id = THREAD_WAITING;
        noza_os.thread[i].join_list.state_id = THREAD_WAITING;
    }

    // init all running thread to NULL
    for (int i=0; i<NOZA_OS_NUM_CORES; i++) {
        noza_make_idle_context(i);
    }
}

// TODO: move the service to user space library
void noza_root_task(void *param)
{
    extern void root_task(void *param);
    extern void noza_run_services(); // application code

    noza_run_services();
    root_task(param);
}

static void noza_run()
{
    noza_os_thread_create(noza_root_task, NULL, 0); // create the first thread for name lookup service (user level)
    noza_os_scheduler(); // start os scheduler and never return
}

// Noza OS scheduler
static thread_t *noza_thread_alloc()
{
    if (noza_os.free.count == 0) {
        platform_panic("noza_thread_alloc: no free thread\n");
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

typedef struct {
    uint32_t idle_stack[64];
    uint32_t *idle_stack_ptr;
} idle_task_t;
static idle_task_t idle_task[NOZA_OS_NUM_CORES];

extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

#if 0
static void idle_entry()
{
    for (;;) {
        __wfi(); // idle, power saving
    }
}
#endif

extern uint32_t *platform_build_stack(uint32_t thread_id, uint32_t *stack, uint32_t size, void (*entry)(void *), void *param);
static void noza_make_idle_context(uint32_t core)
{
    idle_task_t *t = &idle_task[core];
    t->idle_stack_ptr = platform_build_stack(0, t->idle_stack, sizeof(t->idle_stack)/sizeof(uint32_t), platform_idle, (void *)0);
}

static void noza_make_app_context(thread_t *th, void (*entry)(void *param), void *param)
{
    th->stack_ptr = platform_build_stack(thread_get_pid(th), (uint32_t *)th->stack_area, NOZA_OS_STACK_SIZE, (void *)entry, param);
}

static uint32_t noza_os_thread_create( void (*entry)(void *param), void *param, uint32_t pri)
{
    thread_t *th = noza_thread_alloc();
    th->info.priority = pri;
    noza_os_add_thread(&noza_os.ready[pri], th);
    noza_make_app_context(th, entry, param);

    return thread_get_pid(th);
}

//////////////////////////////////////////////////////////////
//
// system call implementation
//
extern void noza_init_kernel_stack(uint32_t *stack);
static void noza_switch_handler(uint32_t core)
{
    uint32_t dummy[32];
    noza_init_kernel_stack(dummy+32);
}

static void syscall_yield(thread_t *running)
{
    noza_os_set_return_value(running, 0);
    noza_os_clear_running_thread(); // force select the next thread
    noza_os_add_thread(&noza_os.ready[running->info.priority], running); // insert the thread back to ready queue
}

static void syscall_sleep(thread_t *running)
{
    running->expired_time = platform_get_absolute_time() + running->trap.r1; // setup expired time
    noza_os_add_thread(&noza_os.sleep, running);
    noza_os_clear_running_thread();
}

static void syscall_thread_create(thread_t *running)
{
    uint32_t ret_value = noza_os_thread_create((void (*)(void *))running->trap.r1, (void *)running->trap.r2, running->trap.r3); 
    noza_os_set_return_value(running, ret_value);
}

static void syscall_thread_join(thread_t *running)
{
    noza_os_join(running, running->trap.r1);
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
    while (running->join_list.count > 0) {
        thread_t *joiner = (thread_t *)running->join_list.head->value;
        noza_os_set_return_value(joiner, 0); // TODO: return the terminate code of running thread
        noza_os_change_state(joiner, &running->join_list, &noza_os.ready[joiner->info.priority]);
    }
    running->info.port_state = PORT_WAIT_LISTEN;
    noza_os_add_thread(&noza_os.free, running);
    if (running == noza_os_get_running_thread())
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
    noza_os_reply(running, running_trap->r1, (void *)running_trap->r2, running_trap->r3);
}

static void syscall_call(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    thread_t *target = &noza_os.thread[running_trap->r1];
    noza_os_send(running, target, (void *)running_trap->r2, running_trap->r3);
}

static void syscall_nbcall(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    thread_t *target = &noza_os.thread[running_trap->r1];
    noza_os_nonblock_send(running, target, (void *)running_trap->r1, running_trap->r2);
}

static void syscall_nbrecv(thread_t *running)
{
    noza_os_nonblock_recv(running);
}

typedef void (*syscall_func_t)(thread_t *source);

static syscall_func_t syscall_func[] = {
    // scheduling
    [NSC_YIELD] = syscall_yield,
    [NSC_SLEEP] = syscall_sleep,
    [NSC_THREAD_CREATE] = syscall_thread_create,
    [NSC_THREAD_CHANGE_PRIORITY] = syscall_thread_change_priority,
    [NSC_THREAD_JOIN] = syscall_thread_join,
    [NSC_THREAD_TERMINATE] = syscall_thread_terminate,

    // messages and ports
    [NSC_RECV] = syscall_recv,
    [NSC_REPLY] = syscall_reply,
    [NSC_CALL] = syscall_call,
    [NSC_NB_RECV] = syscall_nbrecv,
    [NSC_NB_CALL] = syscall_nbcall,
};

static void serv_syscall(uint32_t core)
{
    thread_t *source = noza_os_get_running_thread();
    if (source->trap.r0 >= 0 && source->trap.r0 < NSC_NUM_SYSCALLS) {
        syscall_func_t syscall = syscall_func[source->trap.r0];
        source->trap.state = SYSCALL_SERVING;
        syscall(source);
    } else {
        if (source->trap.r0 == 255) {
            // hardfault
            thread_t *running = noza_os_get_running_thread();
            noza_os_add_thread(&noza_os.hardfault, running); // insert the thread back to ready queue
            noza_os_clear_running_thread(); // remove the running thread
            source->trap.state = SYSCALL_DONE;
        } else {
            source->trap.state = SYSCALL_DONE;
        }
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

    uint32_t now = platform_get_absolute_time();
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

#define SCHEDULE(stack_ptr) \
    noza_os_unlock(core); \
    stack_ptr = noza_os_resume_thread(stack_ptr); \
    core = platform_get_running_core(); \
    noza_os_lock(core); \

static void nozaos_core_dump(thread_t *th)
{
    extern void platform_core_dump(void *stack_ptr);
    platform_core_dump(th->stack_ptr);
}

uint32_t *noza_os_resume_thread(uint32_t *stack);
static void noza_os_scheduler()
{
    uint32_t core = platform_get_running_core();

#if NOZA_OS_NUM_CORES > 1
    platform_multicore_init(noza_os_scheduler);
#endif

    noza_switch_handler(core); // switch to kernel stack (priviliged mode)

    // TODO: for lock, consider the case PendSV interrupt goes here, 
    // and some other high priority interrupt is triggered, could cause deadlock ere
    // maybe need to disable all interrupts here with spin lock
    noza_os_lock(core);
    for (;;) {
        core = platform_get_running_core(); // update the core number whenever the scheduler is called
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
                    extern void platform_trap(void *_stack_ptr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
                    platform_trap(running->stack_ptr, running->trap.r0, running->trap.r1, running->trap.r2, running->trap.r3);
                    running->trap.state = SYSCALL_DONE;
                }
                SCHEDULE(running->stack_ptr)
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
            // no task here, switch to idle thread, and wait for interrupt to wake up the processor
            if (core==0) {
                platform_systick_config(NOZA_OS_TIME_SLICE);
            }

            SCHEDULE(idle_task[core].idle_stack_ptr);
        }

        // process the hardfault thread
        while (noza_os.hardfault.count > 0) {
            printf("process hardfault thread th\n");
            thread_t *th = noza_os.hardfault.head->value;
            nozaos_core_dump(th);
            noza_os_remove_thread(&noza_os.hardfault, th);
            syscall_thread_terminate(th);
        }

        // only core 0 tick the system
        if (core == 0) {
            uint32_t ms = noza_check_sleep_thread(NOZA_OS_TIME_SLICE);
            platform_tick(ms);
        }
        // reschedule the next time slice
    }
}

// software interrupt, system call
// called from assembly SVC0 handler
void noza_os_trap_info(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    uint32_t core = platform_get_running_core();
    // callback from SVC servie routine, make system call pending
    // and trap into kernel later
    noza_os_lock(core);
    thread_t *th = noza_os_get_running_thread();
    if (th != NULL) {
        kernel_trap_info_t *trap = &th->trap;
        trap->r0 = r0;
        trap->r1 = r1;
        trap->r2 = r2;
        trap->r3 = r3;
        trap->state = SYSCALL_PENDING;
    }
    noza_os_unlock(core);

    #if 0 // TODO: issue the PendSV interrupt
    if (r0==255) {
        scb_hw->icsr = M0PLUS_ICSR_PENDSVSET_BITS; // issue PendSV interrupt
    }
    #endif
}

int main()
{
    noza_init();
    noza_run();
}
