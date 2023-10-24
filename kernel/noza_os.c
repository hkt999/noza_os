#include <string.h>
#include <stdio.h>

#include "noza_config.h"
#include "syscall.h"
#include "platform.h"
#include "errno.h"

//#define DEBUG

//////////////////////////////////////////////////////////////
//
// Noza Kernel data structures
//
#define THREAD_FREE             0
#define THREAD_RUNNING          1
#define THREAD_READY            2
#define THREAD_WAITING_MSG      3
#define THREAD_WAITING_READ     4
#define THREAD_WAITING_REPLY    5
#define THREAD_SLEEP            6
#define THREAD_ZOMBIE           7
#define THREAD_PENDING_JOIN     8

#define SYSCALL_DONE            0
#define SYSCALL_PENDING         1
#define SYSCALL_SERVING         2
#define SYSCALL_OUTPUT          3 

#define PORT_WAIT_LISTEN        0
#define PORT_READY              1

#define FLAG_DETACH             0x01

inline static void HALT()
{
    printf("HALT\n");
    for (;;) {
        //__wfi();
    }
}

const char *state_str[] = {
    [THREAD_FREE] = "THREAD_FREE",
    [THREAD_RUNNING] = "THREAD_RUNNING",
    [THREAD_READY] = "THREAD_READY",
    [THREAD_WAITING_MSG] = "THREAD_WAITING_MSG",
    [THREAD_WAITING_READ] = "THREAD_WAITING_READ",
    [THREAD_WAITING_REPLY] = "THREAD_WAITING_REPLY",
    [THREAD_SLEEP] = "THREAD_SLEEP",
    [THREAD_ZOMBIE] = "THREAD_ZOMBIE",
    [THREAD_PENDING_JOIN] = "THREAD_PENDING_JOIN"
};

const char *state_to_str(uint32_t id) 
{
    if (id < sizeof(state_str)/sizeof(char *)) {
        return state_str[id];
    } else {
        return "THREAD_UNKNOWN";
    }
}

const char *port_state_str[] = {
    [PORT_WAIT_LISTEN] = "PORT_WAIT_LISTEN",
    [PORT_READY] = "PORT_READY"
};

const char *port_to_str(uint32_t id) 
{
    if (id < sizeof(port_state_str)/sizeof(char *)) {
        return port_state_str[id];
    } else {
        return "PORT_UNKNOWN";
    }
}

typedef struct cdl_node_s cdl_node_t;
struct cdl_node_s {
    cdl_node_t *next;
    cdl_node_t *prev;
    void *value;
};

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
    uint32_t    priority:8;
    uint32_t    state:8;
    uint32_t    port_state:8;       // PORT_WAIT_LISTEN or PORT_READY
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
    int64_t             expired_time;        // time when the thread expires
    uint32_t            exit_code;           // the exit code
    kernel_trap_info_t  trap;                // kernel trap information
    noza_os_port_t      port;                // port information
    noza_os_message_t   message;             // message passing mechanism for the thread
    thread_t            *join_th;             // thread waiting to join this thread
    uint32_t            stack_area[NOZA_OS_STACK_SIZE]; // stack memory area for the thread
    uint8_t             flags;               // reserved flags
    uint8_t             callid;              // call id  
} thread_t;

// Define a structure for Noza OS management
typedef struct {
    thread_t        *running[NOZA_OS_NUM_CORES];         // array of currently running threads for each core
    thread_list_t   ready[NOZA_OS_PRIORITY_LIMIT];       // array of ready threads for each priority level
    thread_list_t   wait;                                // list of threads in waiting state for thread pending on noza_recv
    thread_list_t   sleep;                               // list of threads in sleeping state
    thread_list_t   zombie;                              // list of threads in zombie state
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
static uint32_t noza_os_thread_create(uint32_t *pth, void (*entry)(void *param), void *param, uint32_t pri);
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

#define UNDEF_REG   -1
inline static void noza_os_set_return_value1(thread_t *th, uint32_t r0)
{
    th->trap.r0 = r0;
    th->trap.r1 = UNDEF_REG;
    th->trap.r2 = UNDEF_REG;
    th->trap.r3 = UNDEF_REG;
    th->trap.state = SYSCALL_OUTPUT;
}

inline static void noza_os_set_return_value2(thread_t *th, uint32_t r0, uint32_t r1)
{
    th->trap.r0 = r0;
    th->trap.r1 = r1;
    th->trap.r2 = UNDEF_REG;
    th->trap.r3 = UNDEF_REG;
    th->trap.state = SYSCALL_OUTPUT;
}

inline static void noza_os_set_return_value3(thread_t *th, uint32_t r0, uint32_t r1, uint32_t r2)
{
    th->trap.r0 = r0;
    th->trap.r1 = r1;
    th->trap.r2 = r2;
    th->trap.r3 = UNDEF_REG;
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
            noza_os_add_thread(&target->port.reply_list, running); // move current running thread to reply list
            // target thread should be in the wait list already, because the port state is PORT_READY
            // so just change the state from wait list to ready list
            if (target->info.state != THREAD_WAITING_MSG) {
                printf("unexpected: target thread (pid:%ld) is not in the waiting list\n", thread_get_pid(target));
                HALT();
            }
            noza_os_change_state(target, &noza_os.wait, &noza_os.ready[target->info.priority]);  // move target from wait list to ready list
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
        noza_os_set_return_value1(running, EAGAIN);
    }
}

static void noza_os_recv(thread_t *running)
{
    if (running->port.reply_list.count > 0) {
        printf("unexpected: running thread (pid:%ld), reply list is not empty, and cannot receive\n", thread_get_pid(running));
        HALT();
    }
    if (running->port.pending_list.count > 0) {
        thread_t *source = running->port.pending_list.head->value; // get the first node in pending_list
        noza_os_set_return_value4(running, 0, thread_get_pid(source), (uint32_t)source->message.ptr, source->message.size); // receive successfully
        noza_os_change_state(source, &running->port.pending_list, &running->port.reply_list); // move from pending list to reply list
    } else {
        // change port stateu to READY and move the running thread to wait list (wait for messages)
        running->info.port_state = PORT_READY; 
        noza_os_add_thread(&noza_os.wait, running);
        noza_os_clear_running_thread();
    }
}

static void noza_os_nonblock_recv(thread_t *running)
{
    if (running->port.pending_list.count > 0) {
        noza_os_recv(running);
    } else {
        noza_os_set_return_value1(running, EAGAIN);
    }
}

static void noza_os_reply(thread_t *running, uint32_t pid, void *msg, uint32_t size)
{
    // sanity check
    if (running->info.port_state != PORT_WAIT_LISTEN) {
        printf("unexpected error: running thread (pid:%ld), port is not PORT_WAIT_LISTEN\n", thread_get_pid(running));
        HALT();
    }
    // search if pid is in the reply list (sanity check)
    // printf("--------------> noza_os_reply to pid=%d\n", pid);
    thread_t *head_th = (thread_t *)running->port.reply_list.head->value;
    thread_t *th = head_th;
    while (th) {
        if (thread_get_pid(th) == pid) {
            break;
        }
        th = th->state_node.next->value;
        if (th == head_th) { // not found in reply list !
            printf("unexpected error: noza_os_reply thread not found in reply list: pid=%ld\n", pid);
            HALT();
            noza_os_set_return_value1(running, ESRCH); // error, not found
            return;
        }
    }

    noza_os_change_state(th, &running->port.reply_list, &noza_os.ready[th->info.priority]);
    noza_os_set_return_value3(th, 0, (uint32_t)msg, size); // 0 -> send/reply success, TODO: think about if error code is needed
    noza_os_set_return_value1(running, 0); // success
    // th->info.port_state = PORT_WAIT_LISTEN;
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
    noza_os.wait.state_id = THREAD_WAITING_MSG;
    noza_os.sleep.state_id = THREAD_SLEEP;
    noza_os.free.state_id = THREAD_FREE;
    noza_os.zombie.state_id = THREAD_ZOMBIE;
//    noza_os.hardfault.state_id = THREAD_HARDFAULT;

    // init all thread structure
    for (i=0; i<NOZA_OS_TASK_LIMIT; i++) {
        noza_os.thread[i].state_node.value = &noza_os.thread[i];
        noza_os_add_thread(&noza_os.free, &noza_os.thread[i]);
        noza_os.thread[i].port.pending_list.state_id = THREAD_WAITING_READ;
        noza_os.thread[i].port.reply_list.state_id = THREAD_WAITING_REPLY;
        noza_os.thread[i].join_th = NULL;
        noza_os.thread[i].callid = -1;
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
    uint32_t th;
    noza_os_thread_create(&th, noza_root_task, NULL, 0); // create the first task (pid:0)
    noza_os_scheduler(); // start os scheduler and never return
}

// Noza OS scheduler
static thread_t *noza_thread_alloc()
{
    if (noza_os.free.count == 0) {
        return (thread_t *)0;
    }

    thread_t *th = (thread_t *)noza_os.free.head->value;
    noza_os_remove_thread(&noza_os.free, th);
    th->flags = 0; // reset the thread flags (says, detached)
    th->callid = -1;

    return th;
}

static void noza_thread_clear_messages(thread_t *th)
{
    // release all pending threads for messages
    while (th->port.pending_list.count>0) {
        thread_t *pending = (thread_t *)th->port.pending_list.head->value;
        noza_os_set_return_value1(pending, EAGAIN);
        noza_os_change_state(pending, &th->port.pending_list, &noza_os.ready[pending->info.priority]);
    }

    while (th->port.reply_list.count>0) {
        thread_t *reply = (thread_t *)th->port.reply_list.head->value;
        noza_os_set_return_value1(reply, EAGAIN);
        noza_os_change_state(reply, &th->port.reply_list, &noza_os.ready[reply->info.priority]);
    }
}

// insert one object to tail and return the head
static cdl_node_t *cdl_add(cdl_node_t *head, cdl_node_t *obj)
{
    if (head == NULL) {
        // 0 element
        obj->next = obj;
        obj->prev = obj;
        return obj;
    } else if (head->next == head) {
        // 1 element
        obj->next = head;
        obj->prev = head;
        head->next = obj;
        head->prev = obj;
        return head;
    }
    // >= 2 elements
    obj->next = head;
    obj->prev = head->prev;
    head->prev->next = obj;
    head->prev = obj;
    return head;
}

static cdl_node_t *cdl_remove(cdl_node_t *head, cdl_node_t *obj)
{
    if (head == obj) {
        if (head->next == head) {
            // only 1 element, and removing head, just clear
            return NULL;
        } else {
            cdl_node_t *new_head;
            if (obj->next == head && obj->prev == head) {
                // only 2 elements, and removing head
                new_head = head->next;
                new_head->prev = new_head->next = new_head;
            } else {
                new_head = head->next;
                new_head->prev = head->prev;
                head->prev->next = new_head;
            }
            return new_head;
        }
    }
    if (obj->next == head && obj->prev == head) {
        // only 2 elements, just update head
        head->next = head->prev = head;
        return head;
    }
    // > 2 element, and removing object
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
    if (thread->info.state != list->state_id) {
        printf("unexpected: thread state is not %s\n", state_to_str(list->state_id));
        HALT();
    }
    list->head = cdl_remove(list->head, &thread->state_node);
    list->count--;
}

typedef struct {
    uint32_t idle_stack[256];
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

static uint32_t noza_os_thread_create(uint32_t *pth, void (*entry)(void *param), void *param, uint32_t pri)
{
    thread_t *th = noza_thread_alloc();
    if (th == NULL) {
        return EAGAIN;
    }
    th->info.priority = pri;
    th->flags = 0x0;
    noza_os_add_thread(&noza_os.ready[pri], th);
    noza_make_app_context(th, entry, param);

    *pth = thread_get_pid(th);
    return 0; // success
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

static void syscall_thread_sleep(thread_t *running)
{
    int64_t duration = ((int64_t)running->trap.r1) << 32 | running->trap.r2;
    running->expired_time = platform_get_absolute_time_us() + duration; // setup expired time
    noza_os_add_thread(&noza_os.sleep, running);
    noza_os_clear_running_thread();
}

static void syscall_thread_kill(thread_t *running)
{
    uint32_t picked = running->trap.r1;
    int sig = running->trap.r2;

    // sanity check
    if (picked >= NOZA_OS_TASK_LIMIT) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    if (picked == thread_get_pid(running)) {
        noza_os_set_return_value1(running, ESRCH);
        return;
    }

    thread_t *th = &noza_os.thread[picked];
    if (th->info.state == THREAD_SLEEP) {
        // move to ready list
        noza_os_change_state(th, &noza_os.sleep, &noza_os.ready[th->info.priority]);
        int64_t now_time = platform_get_absolute_time_us();
        if (now_time < th->expired_time) {
            int64_t remain = th->expired_time - now_time;
            uint32_t high = (uint32_t)(remain >> 32);
            uint32_t low = (uint32_t)(remain & 0xffffffff);
            noza_os_set_return_value3(th, EINTR, high, low);
        } else {
            noza_os_set_return_value3(th, EINTR, 0, 0);
        }
        th->expired_time = 0;
    } else if (th->info.state == THREAD_WAITING_MSG) {
        printf("kernel: kill (%s)\n", state_to_str(th->info.state));
    } else if (th->info.state == THREAD_READY) {
        // TODO: handle ready list (user interrupt)
    } else {
        printf("kernel: (%s) unhandled kill message\n", state_to_str(th->info.state));
    }

    noza_os_set_return_value1(running, 0); // return success
}

static void syscall_thread_create(thread_t *running)
{
    uint32_t th, ret_value = noza_os_thread_create(
        &th,
        (void (*)(void *))running->trap.r1,
        (void *)running->trap.r2,
        running->trap.r3); 
    noza_os_set_return_value2(running, ret_value, th);
}

static void syscall_thread_join(thread_t *running)
{
    // TODO: handle EDEADLK (deadlock)
    uint32_t pid = running->trap.r1;
    // sanity check
    if (pid >= NOZA_OS_TASK_LIMIT) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    thread_t *th = &noza_os.thread[pid];
    if (th->info.state == THREAD_FREE) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    // if the flags is set to detach, then return error (cannot be join)
    if (th->flags & FLAG_DETACH) {
        noza_os_set_return_value1(running, EINVAL); // error
        return;
    }

    // if the thread is in zombie state, then return the exit code, and free the thread
    if (th->info.state == THREAD_ZOMBIE) {
        noza_os_set_return_value2(running, 0, th->exit_code);
        noza_os_change_state(th, &noza_os.zombie, &noza_os.free); // free the thread
        return;
    }

    // TODO: reorg the comparision
    if (th->info.state == THREAD_READY || th->info.state == THREAD_SLEEP || th->info.state == THREAD_WAITING_MSG || th->info.state == THREAD_WAITING_READ || th->info.state == THREAD_WAITING_REPLY) {
        if (th->join_th == NULL) {
            th->join_th = running;
            running->info.state = THREAD_PENDING_JOIN;
            noza_os_clear_running_thread();
        } else {
            noza_os_set_return_value1(running, EINVAL); // already join, return error
        }
    } else {
        printf("unexpected: join thread (%s)\n", state_to_str(th->info.state));
        HALT();
    }
}

static void syscall_thread_detach(thread_t *running)
{
    uint32_t pid = running->trap.r1;
    // sanity check
    if (pid >= NOZA_OS_TASK_LIMIT) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    thread_t *th = &noza_os.thread[pid];
    if (th->info.state == THREAD_FREE) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    // if the thread is in zombie state, then return the exit code, and free the thread
    if (th->info.state == THREAD_ZOMBIE) {
        noza_os_set_return_value1(running, EINVAL);
        noza_os_change_state(th, &noza_os.zombie, &noza_os.free); // free the thread
        return;
    }
    // set the detach flag, and return success
    th->flags |= FLAG_DETACH;
    noza_os_set_return_value1(running, 0); // success
}

static void syscall_thread_change_priority(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;

    // sanity check
    if (running_trap->r1 >= NOZA_OS_TASK_LIMIT) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    thread_t *target = &noza_os.thread[running_trap->r1]; // TODO: introduce the capability and do sanity check
    if (target == running) {
        running->info.priority = running->trap.r2;
        noza_os_set_return_value1(running, 0);
    } else if (target->info.state != THREAD_READY) {
        if (target->info.state == THREAD_FREE || target->info.state == THREAD_ZOMBIE) {
            noza_os_set_return_value1(running, ESRCH); // error
            return;
        }
        target->info.priority = running_trap->r2;
        noza_os_set_return_value1(running, 0);
    } else {
        // target is already in ready list
        noza_os_remove_thread(&noza_os.ready[target->info.priority], target);
        target->info.priority = running_trap->r2;
        noza_os_add_thread(&noza_os.ready[target->info.priority], target);
        noza_os_set_return_value1(running, 0);
    }
}

static void syscall_thread_terminate(thread_t *running)
{
    running->exit_code = running->trap.r1; // r0 is the # of syscall, r1 is the exit code
    running->info.port_state = PORT_WAIT_LISTEN; // TODO: should clear all state if any thread is blocked on this list
    if (running->join_th == NULL) {
        if (running->flags & FLAG_DETACH) {
            // if the thread is detached, then free the thread
            noza_os_add_thread(&noza_os.free, running);
        } else {
            noza_os_add_thread(&noza_os.zombie, running); // move the thread to zombie list
        }
    } else {
        noza_os_add_thread(&noza_os.ready[running->join_th->info.priority], running->join_th); // insert the thread back to ready queue
        noza_os_set_return_value2(running->join_th, 0, running->exit_code);
        running->join_th = NULL; // clear the join thread
        noza_os_add_thread(&noza_os.free, running); // collect the thread to free list
    }
    noza_thread_clear_messages(running); 
    noza_os_clear_running_thread();
}

static void syscall_thread_self(thread_t *running)
{
    noza_os_set_return_value1(running, thread_get_pid(running));
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
    // sanity check
    if (running_trap->r1 >= NOZA_OS_TASK_LIMIT) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    thread_t *target = &noza_os.thread[running_trap->r1];
    noza_os_send(running, target, (void *)running_trap->r2, running_trap->r3);
}

static void syscall_nbcall(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    // sanity check
    if (running_trap->r1 >= NOZA_OS_TASK_LIMIT) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
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
    [NSC_THREAD_SLEEP] = syscall_thread_sleep,
    [NSC_THREAD_KILL] = syscall_thread_kill,
    [NSC_THREAD_CREATE] = syscall_thread_create,
    [NSC_THREAD_CHANGE_PRIORITY] = syscall_thread_change_priority,
    [NSC_THREAD_JOIN] = syscall_thread_join,
    [NSC_THREAD_DETACH] = syscall_thread_detach,
    [NSC_THREAD_TERMINATE] = syscall_thread_terminate,
    [NSC_THREAD_SELF] = syscall_thread_self,

    // messages and ports
    [NSC_RECV] = syscall_recv,
    [NSC_REPLY] = syscall_reply,
    [NSC_CALL] = syscall_call,
    [NSC_NB_RECV] = syscall_nbrecv,
    [NSC_NB_CALL] = syscall_nbcall,
};

const char *syscall_str[] = {
    [NSC_THREAD_SLEEP] = "NSC_THREAD_SLEEP",
    [NSC_THREAD_KILL] = "NSC_THREAD_KILL",
    [NSC_THREAD_CREATE] = "NSC_THREAD_CREATE",
    [NSC_THREAD_CHANGE_PRIORITY] = "NSC_THREAD_CHANGE_PRIORITY",
    [NSC_THREAD_JOIN] = "NSC_THREAD_JOIN",
    [NSC_THREAD_DETACH] = "NSC_THREAD_DETACH",
    [NSC_THREAD_TERMINATE] = "NSC_THREAD_TERMINATE",
    [NSC_THREAD_SELF] = "NSC_THREAD_SELF",
    [NSC_RECV] = "NSC_RECV",
    [NSC_REPLY] = "NSC_REPLY",
    [NSC_CALL] = "NSC_CALL",
    [NSC_NB_RECV] = "NSC_NB_RECV",
    [NSC_NB_CALL] = "NSC_NB_CALL",
};

const char *syscall_to_str(uint32_t callid)
{
    if (callid >= NSC_NUM_SYSCALLS) {
        return "UNKNOWN";
    }
    return syscall_str[callid];
}

static void serv_syscall(uint32_t core)
{
    thread_t *source = noza_os.running[core];
    // sanity check
    if (source->callid >= 0 && source->callid < NSC_NUM_SYSCALLS) {
        /*
        printf("**** syscall: %s, source pid: %d, r0: 0x%08x, r1: 0x%08x, r2: 0x%08x, r3: 0x%08x\n",
            syscall_to_str(source->callid), thread_get_pid(source),
            source->trap.r0, source->trap.r1, source->trap.r2, source->trap.r3);
        */
        source->trap.state = SYSCALL_SERVING;
        syscall_func[source->callid](source); 
    } else {
        if (source->callid == 255) {
            // TODO: handle hardfault
            platform_core_dump(source->stack_ptr, thread_get_pid(source));
            syscall_thread_terminate(source);
        } else {
            // system call not found !
            noza_os_set_return_value1(source, ENOSYS);
        }
    }
}

//////////////////////////////////////////////////////////////

static int64_t noza_check_sleep_thread(int64_t slice) // in us
{
    // TODO: reconsider to rewrite this function for better performance
    uint32_t counter = 0;
    thread_t *expired_thread[NOZA_OS_TASK_LIMIT];

    if (noza_os.sleep.count == 0)
        return slice;

    int64_t now = platform_get_absolute_time_us();
    int64_t min = slice;
    cdl_node_t *cursor = noza_os.sleep.head;
    for (int i=0; i<noza_os.sleep.count; i++) {
        thread_t *th = (thread_t *)cursor->value;
        if (th->expired_time <= now) {
            // expired, move to expired_thread list
            expired_thread[counter++] = th;
            if (counter >= NOZA_OS_TASK_LIMIT) {
                break;
            }
        } else {
            // not expired, calculate the minimum time
            int64_t diff = th->expired_time - now;
            if (diff < (int64_t)min)
                min = diff;
        }
        cursor = cursor->next;
    }
    if (counter>0) {
        for (int i=0; i<counter; i++) {
            thread_t *th = expired_thread[i];
            noza_os_change_state(th, &noza_os.sleep, &noza_os.ready[th->info.priority]);
            noza_os_set_return_value3(th, 0, 0, 0); // return 0, high=0, low=0 successfully timeout
        }
        counter = 0;
    }

    return min;
}

uint32_t *noza_os_resume_thread(uint32_t *stack);
uint32_t *noza_os_resume_thread_syscall(uint32_t *stack);

// switch to user stack
#define SCHEDULE(stack_ptr) \
    noza_os_unlock(core); \
    stack_ptr = noza_os_resume_thread(stack_ptr); \
    noza_os_lock(core); 

#if defined(DEBUG)
static inline void dump_threads()
{
    uint32_t core = platform_get_running_core(); // get working core
    thread_t *running_thread = noza_os.running[core];
    uint32_t total_threads = 0;
    uint32_t ready_count = 0, join_pending_count = 0, waiting_read = 0, waiting_reply = 0;
    if (running_thread) {
        total_threads++;
    }

    for (int i=0; i < NOZA_OS_PRIORITY_LIMIT; i++) {
        if (noza_os.ready[i].count > 0) {
            ready_count += noza_os.ready[i].count;
        }
    }

    for (int i=0; i < NOZA_OS_TASK_LIMIT; i++) {
        if (noza_os.thread[i].join_th) {
            join_pending_count++;
        }
    }

    for (int i=0; i< NOZA_OS_TASK_LIMIT; i++) {
        if (noza_os.thread[i].port.pending_list.count > 0) {
            waiting_read++;
        }
        if (noza_os.thread[i].port.reply_list.count > 0) {
            waiting_reply++;
        }
    }

    total_threads += ready_count + join_pending_count + noza_os.wait.count + noza_os.sleep.count + noza_os.zombie.count + noza_os.free.count + waiting_read + waiting_reply;
    if (total_threads != NOZA_OS_TASK_LIMIT) {
        printf("running: %d, pending_join: %d, ready: %d, wait: %d, sleep: %d, zombie: %d, free:% d, wait_read=%d, wait_reply=%d\n", running_thread ? 1 : 0, 
            join_pending_count, ready_count,
            noza_os.wait.count, noza_os.sleep.count, noza_os.zombie.count, noza_os.free.count, waiting_read, waiting_reply);
        printf("total_threads=%d, NOZA_OS_TASK_LIMIT=%d\n", total_threads, NOZA_OS_TASK_LIMIT);
        for (int i=0; i< NOZA_OS_TASK_LIMIT; i++) {
            thread_t *th = &noza_os.thread[i];
            if (th->info.state != THREAD_FREE) {
                if (th->join_th) {
                    printf("pid: %d, %s, pending_join: %d\n", thread_get_pid(th),
                        state_to_str(th->info.state), thread_get_pid(th->join_th));
                } else {
                    printf("pid: %d, %s\n", thread_get_pid(th), state_to_str(th->info.state));
                }
                if (th->info.state == THREAD_WAITING_REPLY) {
                    printf("\tport status: %s\n", port_to_str(th->info.port_state));
                }
                if (th->port.pending_list.count > 0 || th->port.reply_list.count > 0) {
                    printf("\t* pending_count=%d, reply_count=%d\n", 
                        th->port.pending_list.count,
                        th->port.reply_list.count);
                }
                if (th->port.pending_list.count > 0) {
                    printf("\t\tpedding_list\n");
                    thread_t *pending = (thread_t *)th->port.pending_list.head->value;
                    for (int j = 0; j < th->port.pending_list.count; j++) {
                        printf("\t\tpending (%d) -> %s\n", thread_get_pid(pending), state_to_str(pending->info.state));
                        pending = (thread_t *)pending->state_node.next->value;
                    }
                }
                if (th->port.reply_list.count > 0) {
                    printf("\t\treply_list\n");
                    thread_t *reply = (thread_t *)th->port.reply_list.head->value;
                    for (int j = 0; j<th->port.reply_list.count; j++) {
                        printf("\t\treply (%d) <- %s\n", thread_get_pid(reply), state_to_str(reply->info.state));
                        reply = (thread_t *)reply->state_node.next->value;
                    }
                }
            }
        }
        HALT();
    }
}
#endif

static void noza_os_scheduler()
{
    uint32_t core = platform_get_running_core(); // get working core

#if NOZA_OS_NUM_CORES > 1
    // triger the other core to start and run noza_os_scheduler, too
    platform_multicore_init(noza_os_scheduler);
#endif

    noza_switch_handler(core); // switch to kernel stack (priviliged mode)

    // TODO: for lock, consider the case PendSV interrupt goes here, 
    // and some other high priority interrupt is triggered, could cause deadlock ere
    // maybe need to disable all interrupts here with spin lock
    thread_t *running = NULL;
    noza_os_lock(core);
    for (;;) {
        // core = platform_get_running_core(); // update the core number whenever the scheduler is called
        thread_t *running = NULL;

pick_thread:
        running = NULL;
        // travel the ready queue list and find the highest priority thread
        for (int i=0; i<NOZA_OS_PRIORITY_LIMIT; i++) {
            if (noza_os.ready[i].count > 0) {
                running = noza_os.ready[i].head->value;
                noza_os_remove_thread(&noza_os.ready[i], running);
                break;
            }
        }

        if (running) { // a ready thread is picked
            running->info.state = THREAD_RUNNING;
            noza_os.running[core]= running;
            for (;;) {
                if (running->trap.state == SYSCALL_OUTPUT) {
                    platform_trap(running->stack_ptr, &running->trap);
                    running->trap.state = SYSCALL_DONE;
                    #if defined(DEBUG)
                    void dump_interrupt_stack(uint32_t *stack_ptr, uint32_t callid, uint32_t pid);
                    dump_interrupt_stack(running->stack_ptr, running->callid, thread_get_pid(running));
                    #endif
                    running->callid = -1;
                }

                // TODO: start (for time slice)
                SCHEDULE(running->stack_ptr)
                // TODO: end, calculate the remaining time for slice
                if (running->trap.state == SYSCALL_PENDING) {
                    running->callid = running->trap.r0; // save the callid
                    #if defined(DEBUG)
                    if (running->callid >= NSC_NUM_SYSCALLS && running->callid != 255) {
                        printf("unexpected error: resume fail ! svc: %d (pid:%d)\n",
                            running->callid, thread_get_pid(running));
                    }
                    #endif
                    serv_syscall(core);
                    if (noza_os.running[core] == NULL) {
                        goto pick_thread;
                    }
                } else 
                    break;
            }
            if (noza_os.running[core] != NULL) {
                noza_os_add_thread(&noza_os.ready[noza_os.running[core]->info.priority], noza_os.running[core]);
                noza_os.running[core] = NULL;
            }
        } else {
            #if defined(DEBUG)
            // no task here, switch to idle thread
            int64_t expired = platform_get_absolute_time_us() + noza_check_sleep_thread(NOZA_OS_TIME_SLICE);
            for (;;) {
                int64_t now = platform_get_absolute_time_us();
                if (now >= expired) {
                    break;
                }
                //dump_threads();
            }
            #else
            // no task here, switch to idle thread, and config the next tick
            if (core == 0) {
                int64_t us = noza_check_sleep_thread(NOZA_OS_TIME_SLICE);
                if (us < 200) {
                    us = 200;
                }
                platform_systick_config((uint32_t)us);
            }
            SCHEDULE(idle_task[core].idle_stack_ptr);
            // tick other cores
            if (core == 0) {
                platform_tick_cores();
            }
            #endif
        }
    } // pick a new thread to run, reschedule the next time slice
}

// software interrupt, system call
// called from assembly SVC0 handler, copy register to trap structure
void noza_os_trap_info(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    // callback from SVC servie routine, make system call pending
    // and trap into kernel later
    thread_t *th = noza_os_get_running_thread();
    if (th) {
        kernel_trap_info_t *trap = &th->trap;
        trap->r0 = r0;
        trap->r1 = r1;
        trap->r2 = r2;
        trap->r3 = r3;
        trap->state = SYSCALL_PENDING;
        // TODO: refine the log
        // printf("trap info: r0: 0x%08x, r1: 0x%08x, r2: 0x%08x, r3: 0x%08x\n", r0, r1, r2, r3);
    } else {
        // TODO: unlikely case, no running thread, what to do ? add panic call
        printf("unexpected error, running thread == NULL when trap happen !\n");
    }

    #if 0 // TODO: raise the PendSV interrupt
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
