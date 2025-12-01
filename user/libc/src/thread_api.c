#include <stdio.h>
#include <string.h>
#include "thread_api.h"
#include "service/memory/mem_client.h"
#include "nozaos.h"
#include "kernel/syscall.h"
#include "kernel/noza_config.h"
#include "posix/errno.h"
#include "proc_api.h"
#include "printk.h"

extern uint32_t NOZAOS_PID[NOZA_OS_NUM_CORES];

static hashslot_t THREAD_RECORD_HASH;
static uint32_t g_next_reserved_vid = NOZA_VID_AUTO;

void noza_thread_request_reserved_vid(uint32_t vid)
{
	g_next_reserved_vid = vid;
}
thread_record_t *get_thread_record(uint32_t tid)
{
    return (thread_record_t *)mapping_get_value(&THREAD_RECORD_HASH, tid);
}

extern void app_run(thread_record_t *info, uint32_t pid);

int noza_thread_create(uint32_t *pth, int (*entry)(void *, uint32_t), void *param,
    uint32_t priority, uint32_t stack_size) {
	uint8_t *stack_ptr = (uint8_t *)noza_malloc(stack_size);
	if (stack_ptr == NULL) {
		return EAGAIN;
	}
	return noza_thread_create_with_stack(pth, entry, param, priority, stack_ptr, stack_size, AUTO_FREE_STACK);
}

void noza_thread_exit(uint32_t exit_code) {
	uint32_t pid;
	noza_thread_self(&pid);
	thread_record_t *thread_record = get_thread_record(pid);
	if (thread_record == NULL) {
		printk("fatal: noza_thread_exit: pid %ld not found\n", pid);
		return;
	}
	longjmp(thread_record->jmp_buf, exit_code);
}

void noza_root_task()
{ 
	extern int process_boot(void *param, uint32_t pid);
    mapping_init(&THREAD_RECORD_HASH);
    extern int noza_process_init();
    noza_process_init();

	void *stack_ptr = noza_malloc(NOZA_ROOT_STACK_SIZE);
	thread_record_t *thread_record = (thread_record_t *)stack_ptr;
	thread_record->user_entry = process_boot;
	thread_record->user_param = NULL;
	thread_record->stack_ptr = (uint32_t *)stack_ptr;
	thread_record->stack_size = NOZA_ROOT_STACK_SIZE;
	thread_record->priority = 0;
	thread_record->need_free_stack = AUTO_FREE_STACK;
	thread_record->errno = 0;
	uint32_t tid = 0;
	if (noza_thread_self(&tid) != 0 || tid == 0) {
		for (int core = 0; core < NOZA_OS_NUM_CORES; core++) {
			if (NOZAOS_PID[core] != 0) {
				tid = NOZAOS_PID[core];
				break;
			}
		}
	}
	mapping_insert(&THREAD_RECORD_HASH, tid, &thread_record->hash_item, thread_record);
	noza_thread_detach(tid);
	app_run(thread_record, tid);
}

// thread terminate, call free_stack
uint32_t free_resource(uint32_t tid, uint32_t code)
{
	thread_record_t *thread_record = get_thread_record(tid);
	if (thread_record == NULL) {
		printk("fatal: free_stack: pid %ld not found\n", tid);
		return code;
	}
	if (thread_record->stack_ptr == NULL) {
		printk("fatal: free_stack: pid %ld stack_ptr is NULL\n", tid);
		return code;
	}
    if (((process_record_t *)(thread_record->process))->main_thread == tid) {
        // terminate the main thread, terminate the process
        noza_process_terminate_children_threads(thread_record->process);
    } else {
        noza_process_remove_thread(thread_record->process, tid); 
    }
    mapping_remove(&THREAD_RECORD_HASH, tid);
    noza_free(thread_record->stack_ptr);

	return code;
}

typedef struct {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
} create_thread_t;

int noza_thread_create_with_stack(uint32_t *pth, int (*entry)(void *, uint32_t tid),
	void *param, uint32_t priority, void *user_stack, uint32_t size, uint32_t auto_free_stack)
{
    uint32_t tid;
    noza_thread_self(&tid);
    thread_record_t *me = get_thread_record(tid);

	create_thread_t info;
	thread_record_t *thread_record = (thread_record_t *)user_stack; // stack tail for temporary use
	thread_record->user_entry = entry;
	thread_record->user_param = param;
	thread_record->stack_ptr = (uint32_t *)user_stack;
	thread_record->stack_size = size;
	thread_record->priority = priority;
	thread_record->need_free_stack = auto_free_stack;
	thread_record->errno = 0;
    thread_record->process = me->process;
	thread_record->reserved_vid = g_next_reserved_vid;
	g_next_reserved_vid = NOZA_VID_AUTO;

	// setup register for system call
	info.r0 = NSC_THREAD_CREATE;
	info.r1 = (uint32_t)app_run;
	info.r2 = (uint32_t)thread_record;
	info.r3 = (uint32_t)priority;
	extern void noza_thread_create_primitive(create_thread_t *info);
	noza_thread_create_primitive(&info);
	*pth = info.r1;
    mapping_insert(&THREAD_RECORD_HASH, info.r1, &thread_record->hash_item, thread_record);
    noza_process_add_thread(me->process, info.r1);

	return info.r0;
}
