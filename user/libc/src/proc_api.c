#include <stdio.h>
#include <string.h>
#include "proc_api.h"
#include "spinlock.h"
#include "nozaos.h"
#include "thread_api.h"
#include "service/memory/mem_client.h"
#include "posix/errno.h"
#include "posix/bits/signum.h"
#include "kernel/noza_config.h"

static hashslot_t PROCESS_RECORD_HASH;
static process_record_t *process_head = NULL;
static process_record_t PROCESS_SLOT[NOZA_MAX_PROCESSES];

static env_t default_env = { .argc=1, .argv={"root"} };

static process_record_t *alloc_process_record()
{
	static int process_count = 0;
	noza_raw_lock(&PROCESS_RECORD_HASH.lock);
	process_record_t *process = process_head;
	if (process) {
		process_head = process_head->next;
	}
	noza_spinlock_unlock(&PROCESS_RECORD_HASH.lock);
	// insert the process into hash table
	if (process) {
		mapping_insert(&PROCESS_RECORD_HASH, process_count++, &process->hash_item, process);
	}
	process->env = &default_env;

	return process;
}

static void free_process_record(process_record_t *process)
{
	noza_raw_lock(&PROCESS_RECORD_HASH.lock);
	process->next = process_head;
	process_head = process;
	noza_spinlock_unlock(&PROCESS_RECORD_HASH.lock);
}

int noza_process_init()
{
	mapping_init(&PROCESS_RECORD_HASH);
	for (int i=0; i<NOZA_MAX_PROCESSES-1; i++) {
		PROCESS_SLOT[i].next = &PROCESS_SLOT[i+1];
	}
	PROCESS_SLOT[NOZA_MAX_PROCESSES-1].next = NULL;
	process_head = &PROCESS_SLOT[0];
	return 0;
}

void *pmalloc(size_t size)
{
	uint32_t tid;
	if (noza_thread_self(&tid) == 0) {
		thread_record_t *thread_record = get_thread_record(tid);
		if (thread_record) {
			process_record_t *process = thread_record->process;
			if (process) {
				noza_spinlock_lock(&process->lock);
				void *ptr = ta_alloc(&process->tinyalloc, size);
				noza_spinlock_unlock(&process->lock);
				return ptr;
			} else {
				// unlikely to be here, TODO: exception
			}
		} else {
			// unlikely to be here, TODO: exception
		}
	}
	return NULL;
}

void pfree(void *ptr) 
{
	uint32_t tid;
	if (noza_thread_self(&tid) == 0) {
		thread_record_t *thread_record = get_thread_record(tid);
		if (thread_record) {
			process_record_t *process = get_thread_record(tid)->process;
			if (process) {
				noza_spinlock_lock(&process->lock);
				ta_free(&process->tinyalloc, ptr);
				noza_spinlock_unlock(&process->lock);
			} else {
				// unlikely to be here, TODO: exception
			}
		} else {
			// unlikely to be here, TODO: exception
		}
	}
}

static size_t env_size(env_t *env)
{
	size_t sz = sizeof(int) + env->argc * sizeof(char *);
	for (int i=0; i<env->argc; i++) {
		sz += strlen(env->argv[i]) + 1;
	}

	sz = 32 * ((sz + 31)/32); // align to 32 bytes

	return sz;
}

static env_t *env_copy(tinyalloc_t *ta, env_t *env)
{
	size_t sz = env_size(env);
	env_t *new_env = (env_t *)ta_alloc(ta, sz);
	if (new_env == NULL) {
		return NULL;
	}
	new_env->argc = env->argc;
	char *ptr = (char *)new_env + sizeof(int) + env->argc * sizeof(char *);
	for (int i=0; i<env->argc; i++) {
		new_env->argv[i] = ptr;
		strcpy(ptr, env->argv[i]);
		ptr += strlen(ptr) + 1;
	}
	return new_env;
}

int noza_process_crt0(void *param, uint32_t tid)
{
	process_record_t *process = (process_record_t *)param;
	if (noza_thread_self(&tid) != 0) {
		// unlikely to be here
		printf("fatal: noza_process_crt0: noza_thread_self failed\n");
		return -1;

	}
	thread_record_t *main_thread = get_thread_record(tid);
	if (main_thread == NULL) {
		// unlikely to be here
		printf("fatal: noza_process_crt0: get_thread_record failed\n");
		return -1;
	}
	main_thread->process = process;
	process->heap = noza_malloc(NOZA_PROCESS_HEAP_SIZE); // TODO: use a customized size heap
	// initial heap
	ta_init(&process->tinyalloc, process->heap, process->heap + NOZA_PROCESS_HEAP_SIZE, 256, 16, 8);
	process->env = env_copy(&process->tinyalloc, process->env); // duplicate & copy into process heap
	int ret =  process->main_func(process->env->argc, process->env->argv);
	noza_free(process->heap);

	return ret;
}

int noza_process_exec(main_t entry, int argc, char **argv) {
	uint32_t tid = 0;
	if (process_head == NULL) {
		printf("fatal: process_create: no more process slot\n");
		return -1;
	}

	process_record_t *process = process_head;
	memset(process, 0, sizeof(process_record_t));
	process->main_func = entry;
	noza_thread_create(&tid, noza_process_crt0, (void *)process, 0, 1024);  // TODO: consider the stack size
}

process_record_t *noza_process_self() // TODO: return process is instead of process_t
{
	uint32_t tid;
	if (noza_thread_self(&tid) == 0) {
		thread_record_t *thread_record = get_thread_record(tid);
		if (thread_record) {
			return thread_record->process;
		} else {
			// unlikely to be here, TODO: exception
		}
	}

	// unlikely to be here 
	return NULL;
}

// called from app_run.S
// thread_record points to the stack tail
uint32_t save_exit_context(thread_record_t *thread_record, uint32_t pid)
{
	return setjmp(thread_record->jmp_buf);
}

int noza_process_add_thread(process_record_t *process, uint32_t tid)
{
	if (process->thread_count >= NOZA_PROC_THREAD_COUNT) {
		printf("fatal: noza_process_add_thread: too many threads\n");
		return ENOMEM;
	}
	process->child_thread[process->thread_count++] = tid;
	return 0;
}

int noza_process_remove_thread(process_record_t *process, uint32_t tid)
{
	for (int i=0; i<process->thread_count; i++) {
		if (process->child_thread[i] == tid) {
			process->child_thread[i] = process->child_thread[process->thread_count-1];
			process->thread_count--;
			return 0; // success
		}
	}
	return ESRCH;
}

int noza_process_terminate_children_threads(process_record_t *process)
{
	uint32_t join_pid[NOZA_PROC_THREAD_COUNT];
	int count = 0;

	noza_raw_lock(&PROCESS_RECORD_HASH.lock);
	for (int i=0; i<process->thread_count; i++) {
		join_pid[count++] = process->child_thread[i];
		noza_thread_kill(process->child_thread[i], SIGKILL); // TODO: implement SIGKILL in kernel
	}
	noza_spinlock_unlock(&PROCESS_RECORD_HASH.lock);
	for (int i=0; i<count; i++) {
		noza_thread_join(join_pid[i], NULL);
	}
	return 0;
}

int process_boot(void *param, uint32_t pid)
{
    extern int user_root_task(int argc, char **argv);
    extern void noza_run_services();

	noza_run_services(); // TODO: move this to somewhere else

    process_record_t *proc = alloc_process_record();
    proc->main_func = user_root_task;
    proc->main_thread = 0; // root
    noza_process_crt0(proc, 0); // 0 --> root thread
    free_process_record(proc);
}
