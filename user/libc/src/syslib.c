#include <stdio.h>
#include <stdlib.h> // TODO: remove malloc and free to use the memory service
#include "nozaos.h"
#include "kernel/syscall.h"
#include "kernel/noza_config.h"
#include "setjmp.h"
#include "posix/errno.h"
#include "spinlock.h"
#include "service/memory/mem_client.h"

#define NO_AUTO_FREE_STACK	0
#define AUTO_FREE_STACK		1

extern int noza_syscall(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

inline static uint8_t hash32to8(uint32_t value) {
    uint8_t byte1 = (value >> 24) & 0xFF; // High byte
    uint8_t byte2 = (value >> 16) & 0xFF; // Mid-high byte
    uint8_t byte3 = (value >> 8) & 0xFF;  // Mid-low byte
    uint8_t byte4 = value & 0xFF;         // Low byte

    // XOR the chunks together
    return (byte1 ^ byte2 ^ byte3 ^ byte4);
}

typedef struct process_record_s {
	struct process_record_s *link;
} process_record_t;


static process_record_t root_process;

typedef struct thread_record_s {
	int (*user_entry)(void *param, uint32_t pid);
	void *user_param;
	uint32_t *stack_ptr;
	uint32_t stack_size;
	uint32_t priority;
	uint32_t need_free_stack;
	uint32_t pid;
	struct thread_record_s *next; // if hash collision
	void *thread_data;
	jmp_buf jmp_buf;
	process_record_t process;
	uint32_t errno;
} thread_record_t;

#define HASH_TABLE_SIZE		256
static spinlock_t thread_record_lock;
static thread_record_t *THREAD_RECORD_HASH[HASH_TABLE_SIZE]; // hash table for thread record -- need protect !!

inline static thread_record_t *get_thread_record(uint32_t pid)
{
	int hash = hash32to8(pid);
	noza_raw_lock(&thread_record_lock); // WARNING: this will cause deadlock
	thread_record_t *thread_record = THREAD_RECORD_HASH[hash];
	while (thread_record) {
		if (thread_record->pid == pid)
			break;

		thread_record = thread_record->next;
	}
	noza_spinlock_unlock(&thread_record_lock);
	return thread_record;
}

inline static uint32_t *get_stack_ptr(uint32_t pid, uint32_t *size)
{
	thread_record_t *record = get_thread_record(pid);
	if (record) {
		*size = record->stack_size;
		return record->stack_ptr;
	}

	return NULL;
}

// TODO: move this to platform/arch
uint32_t noza_get_stack_space() {
    uint32_t pid;
    if (noza_thread_self(&pid) == 0) {
        uint32_t sp;
        asm volatile ("mov %0, sp\n" : "=r" (sp));
		uint32_t stack_size;
        uint32_t stack_bottom = (uint32_t)get_stack_ptr(pid, &stack_size);
		return (stack_bottom + stack_size) - sp;
	} else {
		return 0; // fail
    }
}

extern void app_run(thread_record_t *info);

#define MAX_SERVICES    8 // TODO: move this to noza_config.h
typedef struct service_entry {
	int (*entry)(void *param, uint32_t pid);
	void *stack;
	uint32_t stack_size;
} service_entry_t;

static service_entry_t service_entry[NOZA_MAX_SERVICES];
static int service_count = 0;
void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size)
{
    if (service_count >= MAX_SERVICES) {
        printf("fatal: noza_add_service: too many services\n");
		return;
    }
    service_entry[service_count].entry = entry;
	service_entry[service_count].stack = stack;
	service_entry[service_count].stack_size = stack_size;
	service_count++;
}

void noza_root_process()
{
	// TODO: initial root process, and root thread, and create the first thread
}

void noza_init_root()
{
	#if 0 
	static thread_record_t info; // root thread
	thread_record_t *thread_record = &info;
	thread_record->user_entry = NULL;
	thread_record->user_param = NULL;
	thread_record->stack_ptr = NULL;
	thread_record->stack_size = 0; // TODO: check check
	thread_record->priority = 0;
	thread_record->need_free_stack = 0;
	thread_record->pid = 0; // root task
	thread_record->errno = 0;
	int hash = hash32to8(0);

	noza_spinlock_init(&thread_record_lock);
	//noza_spinlock_lock(&thread_record_lock);
	thread_record->next = THREAD_RECORD_HASH[hash];
	THREAD_RECORD_HASH[hash] = thread_record;
	//noza_spinlock_unlock(&thread_record_lock);
	#else
	noza_spinlock_init(&thread_record_lock);
	#endif
}

void noza_free_root()
{
	int hash = hash32to8(0);
	noza_raw_lock(&thread_record_lock);
	thread_record_t *thread_record = THREAD_RECORD_HASH[hash];
	if (thread_record) {
		THREAD_RECORD_HASH[hash] = thread_record->next;
	}
	noza_spinlock_unlock(&thread_record_lock);
}

#define SERVICE_PRIORITY	0
void noza_run_services()
{
    // initial all registered service
    for (int i = 0; i < service_count; i++) {
		uint32_t th;
		noza_thread_create_with_stack(&th, service_entry[i].entry, NULL, SERVICE_PRIORITY,
			service_entry[i].stack, service_entry[i].stack_size, NO_AUTO_FREE_STACK);
    }
}

// called from app_run.S
// thread_record points to the stack tail
uint32_t save_exit_context(thread_record_t *thread_record, uint32_t pid)
{
	return setjmp(thread_record->jmp_buf);
}

// thread terminate, call free_stack
uint32_t free_stack(uint32_t pid, uint32_t code)
{
	thread_record_t *thread_record = get_thread_record(pid);
	if (thread_record == NULL) {
		printf("fatal: free_stack: pid %ld not found\n", pid);
		return code;
	}
	if (thread_record->stack_ptr == NULL) {
		printf("fatal: free_stack: pid %ld stack_ptr is NULL\n", pid);
		return code;
	}

	// remove the thread record from hash table
	int hash = hash32to8(pid);
	noza_raw_lock(&thread_record_lock);
	thread_record_t *prev = NULL;
	thread_record_t *current = THREAD_RECORD_HASH[hash];
	while (current) {
		if (current == thread_record) {
			if (prev) {
				prev->next = current->next;
			} else {
				THREAD_RECORD_HASH[hash] = current->next;
			}
			break;
		}
		prev = current;
		current = current->next;
	}
	if (thread_record->need_free_stack == AUTO_FREE_STACK) {
		noza_free(thread_record->stack_ptr);
	}
	noza_spinlock_unlock(&thread_record_lock);

	return code;
}

typedef struct {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
} create_thread_t;

int noza_thread_create_with_stack(uint32_t *pth, int (*entry)(void *, uint32_t pid), void *param, uint32_t priority, void *user_stack, uint32_t size, uint32_t auto_free_stack)
{
	create_thread_t info;
	thread_record_t *thread_record = (thread_record_t *)user_stack; // stack tail for temporary use
	thread_record->user_entry = entry;
	thread_record->user_param = param;
	thread_record->stack_ptr = (uint32_t *)user_stack;
	thread_record->stack_size = size;
	thread_record->priority = priority;
	thread_record->need_free_stack = auto_free_stack;
	thread_record->pid = -1;
	// TODO: initial proxess here
	thread_record->errno = 0;

	// setup register for system call
	info.r0 = NSC_THREAD_CREATE;
	info.r1 = (uint32_t)app_run;
	info.r2 = (uint32_t)thread_record;
	info.r3 = (uint32_t)priority;
	extern void noza_thread_create_primitive(create_thread_t *info);
	noza_thread_create_primitive(&info);
	thread_record->pid = info.r1;
	*pth = info.r1;

	// insert the thread record to hash table
	int hash = hash32to8(info.r1);
	noza_raw_lock(&thread_record_lock);
	thread_record->next = THREAD_RECORD_HASH[hash];
	THREAD_RECORD_HASH[hash] = thread_record;
	noza_spinlock_unlock(&thread_record_lock);

	return info.r0;
}

int noza_thread_create(uint32_t *pth, int (*entry)(void *, uint32_t), void *param, uint32_t priority, uint32_t stack_size)
{
	uint8_t *stack_ptr = (uint8_t *)noza_malloc(stack_size);
	if (stack_ptr == NULL) {
		return EAGAIN;
	}
	return noza_thread_create_with_stack(pth, entry, param, priority, stack_ptr, stack_size, AUTO_FREE_STACK);
}

void noza_thread_exit(uint32_t exit_code)
{
	uint32_t pid;
	noza_thread_self(&pid);
	thread_record_t *thread_record = get_thread_record(pid);
	if (thread_record == NULL) {
		printf("fatal: noza_thread_exit: pid %ld not found\n", pid);
		return;
	}
	longjmp(thread_record->jmp_buf, exit_code);
}

typedef struct time64_s {
	uint32_t high;
	uint32_t low;
} time64_t;

int noza_thread_sleep_us(int64_t us, int64_t *remain_us)
{
	time64_t tm;
	time64_t remain;

	tm.high = (uint32_t)(us >> 32);
	tm.low = (uint32_t)(us & 0xFFFFFFFF);

	extern uint32_t __noza_thread_sleep(time64_t *tm, time64_t *remain); // in assembly
	uint32_t ret = __noza_thread_sleep(&tm, &remain);

	if (remain_us) {
		*remain_us = ((uint64_t)remain.high << 32) | remain.low;
	}

	return ret;
}

int noza_thread_sleep_ms(int64_t ms, int64_t *remain_ms)
{
	int ret = noza_thread_sleep_us(ms * 1000, remain_ms);
	if (remain_ms) {
		*remain_ms /= 1000;
	}
	return ret;
}

int noza_set_errno(int errno)
{
	uint32_t pid;
	if (noza_thread_self(&pid) == 0) {
		thread_record_t *record = get_thread_record(pid);
		if (record == NULL) {
			printf("fatal: noza_set_errno: pid %ld not found\n", pid);
			return -1; // TODO: return errno
		}
		record->errno = errno;
		return 0;
	}

	return -1;
}

int noza_errno()
{
	uint32_t pid;
	if (noza_thread_self(&pid) == 0) {
		thread_record_t *th = get_thread_record(pid);
		if (th == NULL) {
			printf("fatal: noza_errno: pid %ld not found\n", pid);
			return -1; // TODO: return errno
		}
		return th->errno;
	}

	return -1;
}

int noza_thread_self(uint32_t *pid)
{
	extern uint32_t NOZAOS_PID[NOZA_OS_NUM_CORES];

	uint32_t sp;
	asm volatile ("mov %0, sp\n" : "=r" (sp));
	for (int core = 0; core < NOZA_OS_NUM_CORES; core++) {
		uint32_t noza_pid = NOZAOS_PID[core];
		thread_record_t *thread_record = get_thread_record(noza_pid);
		if (thread_record) {
			if ((uint32_t)thread_record->stack_ptr <= sp && sp < (uint32_t)thread_record->stack_ptr + thread_record->stack_size) {
				*pid = noza_pid;
				return 0; // success
			}
		}
	}

	return -1;
}
