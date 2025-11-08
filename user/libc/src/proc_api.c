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

static void process_record_reset(process_record_t *process)
{
	if (process == NULL)
		return;
	process->lock.num = -1;
	process->lock.spinlock = NULL;
	process->lock.lock_thread = -1;
	memset(&process->hash_item, 0, sizeof(process->hash_item));
#if NOZA_PROCESS_USE_TLSF
	process->tlsf = NULL;
#else
	memset(&process->tinyalloc, 0, sizeof(process->tinyalloc));
#endif
	process->entry = NULL;
	process->main_thread = 0;
	process->thread_count = 0;
	memset(process->child_thread, 0, sizeof(process->child_thread));
	process->heap = NULL;
	process->env = (env_t *)process->env_buf;
	memset(process->env_buf, 0, sizeof(process->env_buf));
	process->next = NULL;
}

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
		process_record_reset(process);
		mapping_insert(&PROCESS_RECORD_HASH, process_count++, &process->hash_item, process);
		noza_spinlock_init(&process->lock);
	}

	return process;
}

static void free_process_record(process_record_t *process)
{
	mapping_remove(&PROCESS_RECORD_HASH, process->hash_item.id);
	if (process->lock.spinlock) {
		noza_spinlock_free(&process->lock);
	}
	process_record_reset(process);
	noza_raw_lock(&PROCESS_RECORD_HASH.lock);
	process->next = process_head;
	process_head = process;
	noza_spinlock_unlock(&PROCESS_RECORD_HASH.lock);
}

int noza_process_init()
{
	mapping_init(&PROCESS_RECORD_HASH);
	for (int i=0; i<NOZA_MAX_PROCESSES; i++) {
		process_record_reset(&PROCESS_SLOT[i]);
		if (i < NOZA_MAX_PROCESSES - 1) {
			PROCESS_SLOT[i].next = &PROCESS_SLOT[i+1];
		} else {
			PROCESS_SLOT[i].next = NULL;
		}
	}
	process_head = &PROCESS_SLOT[0];
	return 0;
}

static size_t calc_env_size(int argc, char *argv[])
{
	size_t sz = sizeof(int) + argc * sizeof(char *);
	for (int i=0; i<argc; i++) {
		sz += strlen(argv[i]) + 1;
	}
	sz = 16 * ((sz + 15)/16); // align to 32 bytes
	return sz;
}

static void env_copy(env_t *dst, int argc, char *argv[])
{
	dst->argc = argc;
	char *ptr = (char *)dst + sizeof(int) + argc * sizeof(char *);
	for (int i=0; i<argc; i++) {
		dst->argv[i] = ptr;
		strcpy(ptr, argv[i]);
		ptr += strlen(ptr) + 1;
	}
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
	process->main_thread = tid;
	main_thread->process = process; // setup process pointer

	// initialize process heap
	process->heap = NULL;
	int ret =  process->entry(process->env->argc, process->env->argv);
	if (process->heap) {
		noza_free(process->heap); // release process heap
		process->heap = NULL;
#if NOZA_PROCESS_USE_TLSF
		process->tlsf = NULL;
#endif
	}
	free_process_record(process); // release this process record

	return ret;
}

int _noza_process_exec(main_t entry, int argc, char *argv[], uint32_t *tid)
{
	*tid = 0;
	process_record_t *process = alloc_process_record();
	if (process == NULL)
		return ENOMEM;

	process->entry = entry;
	size_t required_env = calc_env_size(argc, argv);
	if (required_env > sizeof(process->env_buf)) {
		free_process_record(process);
		return ENOMEM;
	}
	env_copy(process->env, argc, argv);
	int create_ret = noza_thread_create(tid, noza_process_crt0, (void *)process, 0, 1024); // TODO: consider the stack size
	if (create_ret != 0) {
		free_process_record(process);
		return create_ret;
	}

	return 0;
}

int noza_process_exec_detached(main_t entry, int argc, char *argv[])
{
	uint32_t tid;
	return _noza_process_exec(entry, argc, argv, &tid);
}

int noza_process_exec(main_t entry, int argc, char *argv[], int *exit_code)
{
	uint32_t tid;
	*exit_code = 0;
	if (_noza_process_exec(entry, argc, argv, &tid) == 0) {
		if (noza_thread_join(tid, (uint32_t *)exit_code) != 0) {
			return ENOMEM;
		}
		return 0;
	}

	return ENOMEM;
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
	int ret = noza_spinlock_lock(&process->lock);
	if (ret != 0)
		return ret;

	if (process->thread_count >= NOZA_PROC_THREAD_COUNT) {
		printf("fatal: noza_process_add_thread: too many threads\n");
		ret = ENOMEM;
	} else {
		process->child_thread[process->thread_count++] = tid;
	}
	noza_spinlock_unlock(&process->lock);
	return ret;
}

int noza_process_remove_thread(process_record_t *process, uint32_t tid)
{
	int ret = noza_spinlock_lock(&process->lock);
	if (ret != 0)
		return ret;

	for (int i=0; i<process->thread_count; i++) {
		if (process->child_thread[i] == tid) {
			process->child_thread[i] = process->child_thread[process->thread_count-1];
			process->thread_count--;
			process->child_thread[process->thread_count] = 0;
			noza_spinlock_unlock(&process->lock);
			return 0; // success
		}
	}
	noza_spinlock_unlock(&process->lock);
	return ESRCH;
}

int noza_process_terminate_children_threads(process_record_t *process)
{
	uint32_t join_pid[NOZA_PROC_THREAD_COUNT];
	int count = 0;

	int ret = noza_spinlock_lock(&process->lock);
	if (ret != 0)
		return ret;

	for (int i=0; i<process->thread_count; i++) {
		join_pid[count++] = process->child_thread[i];
	}
	process->thread_count = 0;
	memset(process->child_thread, 0, sizeof(process->child_thread));
	noza_spinlock_unlock(&process->lock);

	for (int i=0; i<count; i++) {
		noza_thread_kill(join_pid[i], SIGKILL); // TODO: implement SIGKILL in kernel
	}
	for (int i=0; i<count; i++) {
		noza_thread_join(join_pid[i], NULL);
	}
	return 0;
}

extern int service_main(int argc, char *argv[]);
static int boot2(int argc, char *argv[])
{
    extern int user_root_task(int argc, char *argv[]);
	noza_process_exec_detached(service_main, argc, argv);
	user_root_task(argc, argv);
}

int process_boot(void *param, uint32_t pid)
{

	process_record_t *process = alloc_process_record();
	if (process == NULL)
		return ENOMEM;

	process->entry = boot2;
	process->env->argc = 1;
	process->env->argv[0] = "root";
    noza_process_crt0(process, 0); // 0 --> root thread
}
