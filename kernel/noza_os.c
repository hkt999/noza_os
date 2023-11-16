#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "noza_config.h"
#include "syscall.h"
#include "platform.h"
#include "posix/bits/signum.h"
#include "posix/errno.h"

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

#define NONE_DETACH             0
#define FLAG_DETACH             0x01

inline static uint8_t hash32to8(uint32_t value) {
    uint8_t byte1 = (value >> 24) & 0xFF; // High byte
    uint8_t byte2 = (value >> 16) & 0xFF; // Mid-high byte
    uint8_t byte3 = (value >> 8) & 0xFF;  // Mid-low byte
    uint8_t byte4 = value & 0xFF;         // Low byte

    // XOR the chunks together
    return byte1 ^ byte2 ^ byte3 ^ byte4;
}

inline static void HALT()
{
    printf("HALT\n");
    for (;;) { }
}

static char buffer[128];
void kernel_log(const char *fmt, ...)
{
    #if 0
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    printf("log: %s", buffer);
    #endif
}

void kernel_panic(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    printf("kernel panic: %s", buffer);
    HALT();
}

// share area between kernel and user
// kernel update the data, and user read only

uint32_t NOZAOS_PID[NOZA_OS_NUM_CORES];

const char *state_to_str(uint32_t id) 
{
    static const char *state_str[] = {
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
    if (id < sizeof(state_str)/sizeof(char *)) {
        return state_str[id];
    } else {
        return "THREAD_UNKNOWN";
    }
}

const char *syscall_state_to_str(uint32_t id) 
{
    static const char *syscall_state[] = {
        [SYSCALL_DONE] = "SYSCALL_DONE",
        [SYSCALL_PENDING] = "SYSCALL_PENDING",
        [SYSCALL_SERVING] = "SYSCALL_SERVING",
        [SYSCALL_OUTPUT] = "SYSCALL_OUTPUT"
    };
    if (id < sizeof(syscall_state)/sizeof(char *)) {
        return syscall_state[id];
    } else {
        return "SYSCALL_UNKNOWN";
    }
}

#ifdef DEBUG
const char *port_to_str(uint32_t id) 
{
    static const char *port_state_str[] = {
        [PORT_WAIT_LISTEN] = "PORT_WAIT_LISTEN",
        [PORT_READY] = "PORT_READY"
    };
    if (id < sizeof(port_state_str)/sizeof(char *)) {
        return port_state_str[id];
    } else {
        return "PORT_UNKNOWN";
    }
}

const char *syscall_to_str(uint32_t callid)
{
    static const char *syscall_str[] = {
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
    if (callid >= NSC_NUM_SYSCALLS) {
        return "UNKNOWN";
    }
    return syscall_str[callid];
}
#endif

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
// thread control block (TCB)
typedef struct thread_s {
    uint32_t            *stack_ptr;          // pointer to the thread's stack
    cdl_node_t          state_node;          // node for managing the thread state
    info_t              info;                // thread information
    int64_t             expired_time;        // time when the thread expires
    uint32_t            exit_code;           // the exit code
    uint32_t            vid;                 // virtual id
    kernel_trap_info_t  trap;                // kernel trap information
    noza_os_port_t      port;                // port information
    noza_os_message_t   message;             // message passing mechanism for the thread
    thread_t            *join_th;             // thread waiting to join this thread
    uint32_t            stack_area[NOZA_OS_STACK_SIZE]; // stack memory area for the thread
    uint8_t             flags;               // reserved flags
    uint8_t             callid;              // system call id  
} thread_t;

typedef struct vid_map_item_s {
    struct vid_map_item_s *next;
    uint32_t    pid;
    uint32_t    vid;
} vid_map_item_t;

#define NUM_VID_SLOT    256
// Define a structure for Noza OS management
typedef struct {
    thread_t        *running[NOZA_OS_NUM_CORES];         // array of currently running threads for each core
    thread_list_t   ready[NOZA_OS_PRIORITY_LIMIT];       // array of ready threads for each priority level
    thread_list_t   wait;                                // list of threads in waiting state for thread pending on noza_recv
    thread_list_t   sleep;                               // list of threads in sleeping state
    thread_list_t   zombie;                              // list of threads in zombie state
    thread_list_t   free;                                // list of free/available threads
    thread_t        thread[NOZA_OS_TASK_LIMIT];          // array of thread_t structures for task management
    vid_map_item_t  vid_map[NOZA_OS_TASK_LIMIT];
    vid_map_item_t  *free_vid_map;
    vid_map_item_t  *slot[NUM_VID_SLOT];
} noza_os_t;


/////////////////////////////////////////////////////////////////////////////////////////
//
// declare the Noza kernel data structure instance
//
static noza_os_t noza_os;
inline static uint32_t _thread_get_pid(thread_t *th)
{
    return (th - (thread_t *)&noza_os.thread[0]);
}

inline static uint32_t thread_get_vid(thread_t *th)
{
    return th->vid;
}

inline static int vid_to_pid(uint32_t vid)
{
    vid_map_item_t *vp_map = noza_os.slot[hash32to8(vid)];
    while (vp_map) {
        if (vp_map->vid == vid) {
            return vp_map->pid;
        }
        vp_map = vp_map->next;
    }
    return -1; // not found
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

#if defined(DEBUG)
inline static void dump_threads()
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
    kernel_log("running: %d, pending_join: %d, ready: %d, wait: %d, sleep: %d, zombie: %d, free:% d, wait_read=%d, wait_reply=%d\n", running_thread ? 1 : 0, 
        join_pending_count, ready_count,
        noza_os.wait.count, noza_os.sleep.count, noza_os.zombie.count, noza_os.free.count, waiting_read, waiting_reply);
    kernel_log("total_threads=%d, NOZA_OS_TASK_LIMIT=%d\n", total_threads, NOZA_OS_TASK_LIMIT);
    for (int i=0; i< NOZA_OS_TASK_LIMIT; i++) {
        thread_t *th = &noza_os.thread[i];
        if (th->info.state != THREAD_FREE) {
            if (th->join_th) {
                kernel_log("pid: %d, %s, pending_join: %d\n", thread_get_pid(th),
                    state_to_str(th->info.state), thread_get_pid(th->join_th));
            } else {
                kernel_log("pid: %d, %s", thread_get_pid(th), state_to_str(th->info.state));
            }
            if (th->info.state == THREAD_WAITING_REPLY) {
                kernel_log("\tport status: %s", port_to_str(th->info.port_state));
            }
            if (th->port.pending_list.count > 0 || th->port.reply_list.count > 0) {
                kernel_log("\t* pending_count=%d, reply_count=%d", 
                    th->port.pending_list.count,
                    th->port.reply_list.count);
            }
            if (th->port.pending_list.count > 0) {
                kernel_log("\t\tpedding_list");
                thread_t *pending = (thread_t *)th->port.pending_list.head->value;
                for (int j = 0; j < th->port.pending_list.count; j++) {
                    kernel_log("\t\tpending (%d) -> %s", thread_get_pid(pending), state_to_str(pending->info.state));
                    pending = (thread_t *)pending->state_node.next->value;
                }
            }
            if (th->port.reply_list.count > 0) {
                kernel_log("\t\treply_list");
                thread_t *reply = (thread_t *)th->port.reply_list.head->value;
                for (int j = 0; j<th->port.reply_list.count; j++) {
                    kernel_log("\t\treply (%d) <- %s", thread_get_pid(reply), state_to_str(reply->info.state));
                    reply = (thread_t *)reply->state_node.next->value;
                }
            }
        }
    }
}
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
    if (target->info.state == THREAD_FREE || target->info.state == THREAD_ZOMBIE) {
        noza_os_set_return_value1(running, ESRCH); // error, not found
        return;
    }

    running->message.pid.target = thread_get_vid(target); 
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
                kernel_panic("unexpected: target thread (pid:%ld) is not in the waiting list\n", thread_get_vid(target));
            }
            noza_os_change_state(target, &noza_os.wait, &noza_os.ready[target->info.priority]);  // move target from wait list to ready list
            noza_os_set_return_value4(target, 0, thread_get_vid(running), (uint32_t) msg, size); // 0 --> success
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
    if (running->port.pending_list.count > 0) {
        thread_t *source = running->port.pending_list.head->value; // get the first node in pending_list
        noza_os_set_return_value4(running, 0, thread_get_vid(source), (uint32_t)source->message.ptr, source->message.size); // receive successfully
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

static void noza_os_reply(thread_t *running, uint32_t vid, void *msg, uint32_t size)
{
    // sanity check
    if (running->info.port_state != PORT_WAIT_LISTEN) {
        kernel_panic("unexpected: running thread (pid:%ld), port is not PORT_WAIT_LISTEN\n", thread_get_vid(running));
    }
    // search if pid is in the reply list (sanity check)
    thread_t *head_th = (thread_t *)running->port.reply_list.head->value;
    thread_t *th = head_th;
    while (th) {
        if (thread_get_vid(th) == vid) {
            break;
        }
        th = th->state_node.next->value;
        if (th == head_th) { // not found in reply list !
            kernel_panic("unexpected: noza_os_reply thread not found in reply list: pid=%ld\n", vid);
            noza_os_set_return_value1(running, ESRCH); // error, not found
            return;
        }
    }

    if (th->info.state == THREAD_FREE || th->info.state == THREAD_ZOMBIE) {
        noza_os_set_return_value1(running, ESRCH); // error, not found
        return;
    }

    noza_os_change_state(th, &running->port.reply_list, &noza_os.ready[th->info.priority]);
    noza_os_set_return_value3(th, 0, (uint32_t)msg, size); // 0 -> send/reply success
    noza_os_set_return_value1(running, 0); // success
}

static void noza_make_idle_context(uint32_t core);

// start the noza os
static void noza_init()
{
    platform_io_init(); // initial all state in platform

    // init noza os / thread structure
    memset(&noza_os, 0, sizeof(noza_os_t));
    noza_os_lock_init();
    for (int i=0; i<NOZA_OS_PRIORITY_LIMIT; i++) {
        noza_os.ready[i].state_id = THREAD_READY;
    }
    noza_os.wait.state_id = THREAD_WAITING_MSG;
    noza_os.sleep.state_id = THREAD_SLEEP;
    noza_os.free.state_id = THREAD_FREE;
    noza_os.zombie.state_id = THREAD_ZOMBIE;

    // init all thread structure
    for (int i=0; i<NOZA_OS_TASK_LIMIT; i++) {
        noza_os.thread[i].state_node.value = &noza_os.thread[i];
        noza_os_add_thread(&noza_os.free, &noza_os.thread[i]);
        noza_os.thread[i].port.pending_list.state_id = THREAD_WAITING_READ;
        noza_os.thread[i].port.reply_list.state_id = THREAD_WAITING_REPLY;
        noza_os.thread[i].join_th = NULL;
        noza_os.thread[i].callid = -1;
    }

    // init all vid slots
    for (int i=0; i<NOZA_OS_TASK_LIMIT-1; i++) {
        noza_os.vid_map[i].next = &noza_os.vid_map[i+1];
    }
    noza_os.vid_map[NOZA_OS_TASK_LIMIT-1].next = NULL;
    noza_os.free_vid_map = &noza_os.vid_map[0];

    // init all running thread to NULL
    for (int i=0; i<NOZA_OS_NUM_CORES; i++) {
        noza_make_idle_context(i);
    }
}

extern void noza_root_task(); // ini syslib.c
static void noza_run()
{
    uint32_t th;
    noza_os_thread_create(&th, noza_root_task, NULL, 0); // create the first task (pid:0) 
    // TODO: set th as detach, after creating the first user process, just exit
    noza_os_scheduler(); // start os scheduler and never return
}

inline static void noza_os_insert_vid(thread_t *src_th)
{
    // add virtual pid slot
    vid_map_item_t *vp_map = noza_os.free_vid_map;
    if (vp_map==NULL) {
        kernel_panic("fatal error: noza_thread_alloc: free_vid_map is NULL\n");
    }
    noza_os.free_vid_map = vp_map->next;

    static uint32_t ID = 0;
    uint32_t vid = ID++;
    if (ID>=65536)
        ID = 0; // maximun vid: 65535

    vp_map->vid = vid;
    vp_map->pid = _thread_get_pid(src_th);
    src_th->vid = vid;
    int hash = hash32to8(vid);
    vp_map->next = noza_os.slot[hash]; // insert to the head of the list
    noza_os.slot[hash] = vp_map;
}

inline static void noza_os_remove_vid(thread_t *src_th)
{
    uint32_t pid = _thread_get_pid(src_th);
    int hash = hash32to8(src_th->vid);

    vid_map_item_t *prev = NULL;
    vid_map_item_t *vp_map = noza_os.slot[hash];
    while (vp_map) {
        if (vp_map->pid == pid) { // match
            if (prev != NULL) {
                prev->next = vp_map->next;
            } else {
                noza_os.slot[hash] = vp_map->next;
            }
            vp_map->next = noza_os.free_vid_map;
            noza_os.free_vid_map = vp_map;
            break;
        }
        prev = vp_map;
        vp_map = vp_map->next;
    }
}

// Noza OS scheduler
inline static thread_t *noza_thread_alloc()
{
    if (noza_os.free.count == 0) {
        return (thread_t *)0;
    }

    thread_t *th = (thread_t *)noza_os.free.head->value;
    noza_os_remove_thread(&noza_os.free, th);
    th->flags = NONE_DETACH; // reset the thread flags (says, detached)
    th->callid = -1;
    th->trap.state = SYSCALL_DONE;
    noza_os_insert_vid(th);
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
    if (head == NULL) {
        // 0 element, just return NULL
        kernel_panic("fatal error: cdl_remove: head is NULL\n");
        return NULL;
    }
    if (head == obj) {
        if (head->next == head && head->prev == head) {
            // only 1 element, and removing head, just clear
            return NULL;
        } else {
            // more than 1 element, and removing head
            cdl_node_t *new_head = head->next;
            new_head->prev = head->prev;
            head->prev->next = new_head;
            return new_head;
        }
    } else {
        // more than 2 elements, and removing object
        obj->prev->next = obj->next;
        obj->next->prev = obj->prev;
        return head;
    }
}

static void noza_os_add_thread_by_expired_time(thread_list_t *list, thread_t *thread)
{
    // find a proper place to insert the thread
    int64_t expired_time = thread->expired_time;
    cdl_node_t *node = list->head;
    if (node == NULL) {
        // 0 element
        list->head = &thread->state_node;
        thread->state_node.next = &thread->state_node;
        thread->state_node.prev = &thread->state_node;
    } else if (node->next == node) {
        // 1 element
        node->next = &thread->state_node;
        node->prev = &thread->state_node;
        thread->state_node.next = node;
        thread->state_node.prev = node;
        if (((thread_t *)node->value)->expired_time > expired_time) {
            list->head = &thread->state_node;
        }
    } else {
        // more than 2 elements
        cdl_node_t *tail = node->prev;
        while (node != tail) {
            if (((thread_t *)node->value)->expired_time > expired_time) {
                // insert before node
                thread->state_node.next = node;
                thread->state_node.prev = node->prev;
                node->prev->next = &thread->state_node;
                node->prev = &thread->state_node;
                if (node == list->head) {
                    list->head = &thread->state_node;
                }
                break;
            }
            node = node->next;
        }
        if (node == tail) {
            list->head = cdl_add(list->head, &thread->state_node);
        }
    }
}


static void noza_os_add_thread(thread_list_t *list, thread_t *thread)
{
    if (list->state_id == THREAD_SLEEP) {
        noza_os_add_thread_by_expired_time(list, thread);
    } else {
        list->head = cdl_add(list->head, &thread->state_node);
    }

    list->count++;
    thread->info.state = list->state_id;

    if (list->state_id == THREAD_FREE) {
        noza_os_remove_vid(thread);
    }
}

static void noza_os_remove_thread(thread_list_t *list, thread_t *thread)
{
    if (thread->info.state != list->state_id) {
        kernel_panic("unexpected: thread state is not list:%s, thread:%s\n",
            state_to_str(list->state_id), state_to_str(thread->info.state));
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
extern uint32_t *platform_build_stack(uint32_t thread_id, uint32_t *stack, uint32_t size, void (*entry)(void *), void *param);
static void noza_make_idle_context(uint32_t core)
{
    idle_task_t *t = &idle_task[core];
    t->idle_stack_ptr = platform_build_stack(0, t->idle_stack, sizeof(t->idle_stack)/sizeof(uint32_t), platform_idle, (void *)0);
}

static void noza_make_app_context(thread_t *th, void (*entry)(void *param), void *param)
{
    th->stack_ptr = platform_build_stack(
        thread_get_vid(th), (uint32_t *)th->stack_area, NOZA_OS_STACK_SIZE, (void *)entry, param);
}

static uint32_t noza_os_thread_create(uint32_t *pth, void (*entry)(void *param), void *param, uint32_t priority)
{
    // sanity check
    if (priority > NOZA_OS_PRIORITY_LIMIT) {
        return EINVAL;
    }

    thread_t *th = noza_thread_alloc();
    if (th == NULL) {
        return EAGAIN;
    }
    th->info.priority = priority;
    th->flags = 0x0;
    noza_os_add_thread(&noza_os.ready[priority], th);
    noza_make_app_context(th, entry, param);

    *pth = thread_get_vid(th);
    return 0; // success
}

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
    if (duration <= 0) {
        noza_os_add_thread(&noza_os.ready[running->info.priority], running);
    } else { 
    running->expired_time = platform_get_absolute_time_us() + duration; // setup expired time
        noza_os_add_thread(&noza_os.sleep, running);
    }
    noza_os_clear_running_thread();
}

static void do_nothing_signal_handler(thread_t *running, thread_t *target) {
    // do nothing
}

static void terminate_signal_handler(thread_t *running, thread_t *target) {
    // TODO
    kernel_log("fatal error: unimplement feature (singal to terminate)\n");
}

static void alarm_signal_handler(thread_t *running, thread_t *target)
{
    if (target->info.state == THREAD_SLEEP) {
        // move to ready list
        noza_os_change_state(target, &noza_os.sleep, &noza_os.ready[target->info.priority]);
        int64_t now_time = platform_get_absolute_time_us();
        if (now_time < target->expired_time) {
            int64_t remain = target->expired_time - now_time;
            uint32_t high = (uint32_t)(remain >> 32);
            uint32_t low = (uint32_t)(remain & 0xffffffff);
            noza_os_set_return_value3(target, EINTR, high, low);
        } else {
            noza_os_set_return_value3(target, EINTR, 0, 0);
        }
        target->expired_time = 0;
    } else if (target->info.state == THREAD_WAITING_MSG) {
        noza_os_change_state(target, &noza_os.wait, &noza_os.ready[target->info.priority]);
        noza_os_set_return_value1(target, EINTR);
    } else if (target->info.state == THREAD_WAITING_READ) {
        // target thread is in some thread's port.pending_list
        // TODO: think faster way to remove the thread from pending list
        kernel_log("fatal error: unimplement feature (1)\n");
    } else if (target->info.state == THREAD_WAITING_REPLY) {
        // target thread is in some thread's port.reply_list
        // TODO: think faster way to remove the thread from reply list
        kernel_log("fatal error: unimplement feature (2)\n");
    } else {
        kernel_log("kernel: vid (%d) (%s) unhandled kill message\n", thread_get_vid(target), state_to_str(target->info.state));
        noza_os_set_return_value1(target, ESRCH);
        return;
    }

    noza_os_set_return_value1(running, 0); // return success
}

typedef void (*signal_handler_t)(thread_t *running, thread_t *target);
static signal_handler_t signal_handler[32] = {
    [0] = do_nothing_signal_handler,
    [SIGHUP] = terminate_signal_handler,         // hangup
    [SIGINT] = terminate_signal_handler,         // CTRL-C
    [SIGQUIT] = terminate_signal_handler,        // quit, core dump
    [SIGILL] = terminate_signal_handler,         // illegal instruction, core dump
    [SIGTRAP] = terminate_signal_handler,        // trap, core dump
    [SIGABRT] = terminate_signal_handler,        // core dump (using abort())
    [SIGIOT] = terminate_signal_handler,         // core dump (same as SIGABRT)
    [SIGBUS] = terminate_signal_handler,         // bus error, core dump
    [SIGFPE] = terminate_signal_handler,         // floating point exception, core dump
    [SIGKILL] = terminate_signal_handler,        // kill, cannot be caught
    [SIGUSR1] = terminate_signal_handler,        // user defined signal
    [SIGSEGV] = terminate_signal_handler,        // segmentation fault, core dump
    [SIGUSR2] = terminate_signal_handler,        // user defined signal 2
    [SIGPIPE] = terminate_signal_handler,        // broken pipe
    [SIGALRM] = alarm_signal_handler,            // alarm clock
    [SIGTERM] = terminate_signal_handler,        // software termination signal
    [SIGSTKFLT] = terminate_signal_handler,      // stack fault
    [SIGCHLD] = do_nothing_signal_handler,       // child process terminated, stopped, or continued
    [SIGCONT] = do_nothing_signal_handler,       // continue executing, if stopped -- TODO: move to ready state
    [SIGSTOP] = do_nothing_signal_handler,       // stop executing, cannot be caught -- TODO: move to wait state
    [SIGTSTP] = do_nothing_signal_handler,       // CTRL-Z interactive stop signal, cannot be caught 
    [SIGTTIN] = do_nothing_signal_handler,       // background process attempting read from terminal -- TODO?
    [SIGTTOU] = do_nothing_signal_handler,       // background process attempting write to terminal -- TODO?
    [SIGURG] = do_nothing_signal_handler,        // urgent condition on IO channel
    [SIGXCPU] = terminate_signal_handler,        // CPU time limit exceeded, core dump
    [SIGXFSZ] = terminate_signal_handler,        // file size limit exceeded, core dump
    [SIGVTALRM] = terminate_signal_handler,      // virtual time alarm, terminate
    [SIGPROF] = terminate_signal_handler,        // profiling time alarm, terminate
    [SIGWINCH] = do_nothing_signal_handler,      // window size change
    [SIGIO] = do_nothing_signal_handler,         // IO now possible
    [SIGPWR] = terminate_signal_handler,         // power failure restart
    [SIGSYS] = terminate_signal_handler,         // bad system call, terminate, core dump
    [SIGUNUSED] = do_nothing_signal_handler      // reserved
};

inline static thread_t *get_thread_by_vid(uint32_t vid)
{
    int hash = hash32to8(vid);
    vid_map_item_t *vp_map = noza_os.slot[hash];
    while (vp_map) {
        if (vp_map->vid == vid) {
            return &noza_os.thread[vp_map->pid];
        }
        vp_map = vp_map->next;
    }

    return NULL;
}

static void syscall_thread_kill(thread_t *running)
{
    uint32_t picked = running->trap.r1;
    thread_t *picked_thread = get_thread_by_vid(picked);
    int sig = running->trap.r2;
    if (picked_thread == NULL) {
        noza_os_set_return_value1(running, ESRCH);
        return;
    }
    if (sig > 31) {
        noza_os_set_return_value1(running, EINVAL);
        return;
    }
    signal_handler[sig](running, picked_thread);
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
    uint32_t picked = running->trap.r1; // the pid of the thread to join
    thread_t *target = get_thread_by_vid(picked);
    // sanity check
    if (target == NULL) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    if (target->flags & FLAG_DETACH) {
        noza_os_set_return_value1(running, EINVAL); // error
        return;
    }
    switch (target->info.state) {
        case THREAD_FREE:
            noza_os_set_return_value1(running, ESRCH); // error
            break;
        case THREAD_ZOMBIE:
            noza_os_set_return_value2(running, 0, target->exit_code);
            noza_os_change_state(target, &noza_os.zombie, &noza_os.free); // free the thread
            break;
        case THREAD_RUNNING:
        default:
            if (target->join_th == NULL) {
                target->join_th = running;
                running->info.state = THREAD_PENDING_JOIN;
                noza_os_clear_running_thread();
            } else {
                noza_os_set_return_value1(running, EINVAL); // already join, return error
            }
            break;
    }
}

static void syscall_thread_detach(thread_t *running)
{
    uint32_t picked = running->trap.r1;
    thread_t *target = get_thread_by_vid(picked);
    // sanity check
    if (target == NULL) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    if (target->info.state == THREAD_FREE) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    // if the thread is in zombie state, then return the exit code, and free the thread
    if (target->info.state == THREAD_ZOMBIE) {
        noza_os_set_return_value1(running, EINVAL);
        noza_os_change_state(target, &noza_os.zombie, &noza_os.free); // free the thread
        return;
    }
    // set the detach flag, and return success
    target->flags |= FLAG_DETACH;
    noza_os_set_return_value1(running, 0); // success
}

static void syscall_thread_change_priority(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    thread_t *target = get_thread_by_vid(running_trap->r1);
    // sanity check
    if (target == NULL) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
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
    running->info.port_state = PORT_WAIT_LISTEN;

    // release all pending threads for messages
    if (running->port.pending_list.count > 0) {
        thread_t *head = running->port.pending_list.head->value;
        while (head) {
            noza_os_change_state(head, &running->port.pending_list, &noza_os.ready[head->info.priority]);
            noza_os_set_return_value1(head, EINTR);
            head = running->port.pending_list.head->value;
        }
    }

    // release all pending threads for reply
    if (running->port.reply_list.count > 0) {
        thread_t *head = running->port.reply_list.head->value;
        while (head) {
            noza_os_change_state(head, &running->port.reply_list, &noza_os.ready[head->info.priority]);
            noza_os_set_return_value1(head, EINTR);
            head = running->port.reply_list.head->value;
        }
    }

    // what if the thread is in the some others wait list or reply list ?
    // unlikely to happen, because the thread is in the running state, so that it can call "terminate"
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
    thread_t *target = get_thread_by_vid(running_trap->r1);
    // sanity check
    if (target == NULL) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
    noza_os_send(running, target, (void *)running_trap->r2, running_trap->r3);
}

static void syscall_nbcall(thread_t *running)
{
    kernel_trap_info_t *running_trap = &running->trap;
    thread_t *target = get_thread_by_vid(running_trap->r1);
    // sanity check
    if (target == NULL) {
        noza_os_set_return_value1(running, ESRCH); // error
        return;
    }
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

    // messages and ports
    [NSC_RECV] = syscall_recv,
    [NSC_REPLY] = syscall_reply,
    [NSC_CALL] = syscall_call,
    [NSC_NB_RECV] = syscall_nbrecv,
    [NSC_NB_CALL] = syscall_nbcall,
};

inline static void serv_syscall(uint32_t core)
{
    thread_t *source = noza_os.running[core];
    // sanity check
    if (source->callid >= 0 && source->callid < NSC_NUM_SYSCALLS) {
        source->trap.state = SYSCALL_SERVING;
        syscall_func[source->callid](source); 
    } else {
        if (source->callid == 255) {
            // TODO: handle hardfault
            platform_core_dump(source->stack_ptr, thread_get_vid(source));
            syscall_thread_terminate(source);
        } else {
            // system call not found !
            noza_os_set_return_value1(source, ENOSYS);
        }
        source->trap.state = SYSCALL_DONE;
    }
}

//////////////////////////////////////////////////////////////
inline static uint32_t noza_wakeup(int64_t now)
{
    if (noza_os.sleep.count == 0)
        return 0;

    cdl_node_t *cursor = noza_os.sleep.head;
    while (cursor != NULL) {
        thread_t *th = (thread_t *)cursor->value;
        if (th->expired_time <= now) {
            noza_os_change_state(th, &noza_os.sleep, &noza_os.ready[th->info.priority]);
            noza_os_set_return_value3(th, 0, 0, 0); // return 0, high=0, low=0 successfully timeout
            cursor = noza_os.sleep.head;
        } else
            break;
    }
}

inline static void noza_reconfig_systick(int64_t now)
{
    cdl_node_t *cursor = noza_os.sleep.head;
    if (now == 0 || cursor == NULL) {
        // no thread is sleeping, then set the next tick to 10ms
        platform_systick_config(NOZA_OS_TIME_SLICE);
    } else {
        // some thread is sleeping, then set the next tick to the first thread's expired time
        thread_t *th = (thread_t *)cursor->value;
        platform_systick_config(th->expired_time - now);
    }
}

extern uint32_t *noza_os_resume_thread(uint32_t *stack);
// switch to user stack
inline static void GO_RUN(int core, thread_t *running)
{
    NOZAOS_PID[core] = thread_get_vid(running); // setup the shared variable
    noza_os_unlock(core);
    running->stack_ptr = noza_os_resume_thread(running->stack_ptr);
    noza_os_lock(core); 
}

// switch to idle stack
inline static void GO_IDLE(int core)
{
    noza_os_unlock(core);
    idle_task[core].idle_stack_ptr = noza_os_resume_thread(idle_task[core].idle_stack_ptr);
    noza_os_lock(core);
}

// pick a thread from ready queue base on priority
inline static thread_t *pick_ready_thread()
{
    thread_t *running = NULL;
    // travel the ready queue list and find the highest priority thread
    for (int i=0; i<NOZA_OS_PRIORITY_LIMIT; i++) {
        if (noza_os.ready[i].count > 0) {
            running = noza_os.ready[i].head->value;
            noza_os_remove_thread(&noza_os.ready[i], running);
            break;
        }
    }
    return running;
}

inline static void check_syscall_output(thread_t *running)
{
    if (running->trap.state == SYSCALL_OUTPUT) {
        platform_trap(running->stack_ptr, &running->trap); // copy register to trap structure
        running->trap.state = SYSCALL_DONE; // clear the state
        running->callid = -1; // clear the callid
    }
}

inline static void check_syscall_serve(int core, thread_t *running)
{
    if (running->trap.state == SYSCALL_PENDING) {
        running->callid = running->trap.r0; // save the callid
        serv_syscall(core);
    }
}

static void noza_os_scheduler()
{
    uint32_t core = platform_get_running_core(); // get working core

#if NOZA_OS_NUM_CORES > 1
    // triger the other core to start and run noza_os_scheduler, too
    platform_multicore_init(noza_os_scheduler);
#endif
    noza_switch_handler(core); // switch to kernel stack (priviliged mode)
    platform_systick_config(NOZA_OS_TIME_SLICE); // start system tick 10ms
    // TODO: for lock, consider the case PendSV interrupt goes here, 
    // some other high priority interrupt is triggered, could cause deadlock here
    // maybe need to disable all interrupts here with spin lock
    noza_os_lock(core);
    for (;;) {
        thread_t *running = pick_ready_thread();
        if (running) {
            // a ready thread is picked
            int64_t now = platform_get_absolute_time_us();
            int64_t expired = now + NOZA_OS_TIME_SLICE; // pick up a new thread, and setup time slice
            running->info.state = THREAD_RUNNING;
            noza_os.running[core] = running;
            for (;;) {
                check_syscall_output(running); // check if the call pending on output
                if (expired > now) {
                    GO_RUN(core, running);
                    now = platform_get_absolute_time_us();  // update time
                    check_syscall_serve(core, running);     // check if any syscall is pending, if pending, then serve it
                    if (noza_os.running[core] == NULL)
                        break; 
                } else
                    break;
            }
            running = noza_os.running[core];
            if (running != NULL) {
                noza_os_add_thread(&noza_os.ready[running->info.priority], running);
                noza_os.running[core] = NULL;
            }
        } else {
            int64_t now = platform_get_absolute_time_us();
            if (noza_wakeup(now) == 0) {
                noza_reconfig_systick(now);
                GO_IDLE(core);
                noza_reconfig_systick(0);
                if (core == 0) {
                    platform_tick_cores();
                }
            }
        }
    }
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
    } else {
        kernel_log("unexpected: running thread == NULL when trap happen !\n");
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
