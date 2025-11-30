// NozaOS
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "noza_ipc.h"
#include "noza_fs.h"
#include "spinlock.h"

typedef struct {
    uint32_t    high;
    uint32_t    low;
} noza_time64_t;

#define NO_AUTO_FREE_STACK	0
#define AUTO_FREE_STACK	1
#define NOZA_CLOCK_REALTIME     0
#define NOZA_CLOCK_MONOTONIC    1
#define NOZA_TIMER_FLAG_PERIODIC 0x01

// Noza thread & scheduling
int     noza_thread_sleep_us(int64_t us, int64_t *remain_us);
int     noza_thread_sleep_ms(int64_t ms, int64_t *remain_ms);
int     noza_thread_create(uint32_t *pth, int (*entry)(void *param, uint32_t pid), void *param, uint32_t priority, uint32_t stack_size);
int     noza_thread_create_with_stack(uint32_t *pth, int (*entry)(void *param, uint32_t pid), void *param, uint32_t priority, void *stack_addr, uint32_t stack_size, uint32_t auto_free);
int     noza_thread_kill(uint32_t thread_id, int sig);
int     noza_thread_change_priority(uint32_t thread_id, uint32_t priority);
int     noza_thread_detach(uint32_t thread_id);
int     noza_thread_join(uint32_t thread_id, uint32_t *code);
void    noza_thread_terminate(int exit_code);
int     noza_thread_self(uint32_t *pid);
int     noza_futex_wait(uint32_t *addr, uint32_t expected, int32_t timeout_us);
int     noza_futex_wake(uint32_t *addr, uint32_t count);

// Noza message API
int     noza_recv(noza_msg_t *msg);
int     noza_call(noza_msg_t *msg);
int     noza_reply(noza_msg_t *msg);
int     noza_nonblock_call(noza_msg_t *msg);
int     noza_nonblock_recv(noza_msg_t *msg);
uint32_t noza_get_stack_space();
int     noza_timer_create(uint32_t *timer_id);
int     noza_timer_delete(uint32_t timer_id);
int     noza_timer_arm(uint32_t timer_id, uint32_t duration_us, uint32_t flags);
int     noza_timer_cancel(uint32_t timer_id);
int     noza_timer_wait(uint32_t timer_id, int32_t timeout_us);
int     noza_clock_gettime(uint32_t clock_id, noza_time64_t *timestamp);
int     noza_signal_send(uint32_t tid, uint32_t signum);
uint32_t noza_signal_take(void);

// File system (IPC to fs service)
int     noza_open(const char *path, int flags, int mode);
int     noza_close(int fd);
int     noza_read(int fd, void *buf, uint32_t len);
int     noza_write(int fd, const void *buf, uint32_t len);
int64_t noza_lseek(int fd, int64_t offset, int whence);
int     noza_stat(const char *path, noza_fs_attr_t *st);
int     noza_fstat(int fd, noza_fs_attr_t *st);
int     noza_mkdir(const char *path, uint32_t mode);
int     noza_unlink(const char *path);
int     noza_chdir(const char *path);
char   *noza_getcwd(char *buf, size_t size);
uint32_t noza_umask(uint32_t new_mask);
int     noza_chmod(const char *path, uint32_t mode);
int     noza_chown(const char *path, uint32_t uid, uint32_t gid);
int     noza_opendir(const char *path);
int     noza_closedir(int dir_fd);
int     noza_readdir(int dir_fd, noza_fs_dirent_t *ent, int *at_end);

// user level call
int noza_set_errno(int errno);
int noza_get_errno();

// process
typedef int (*main_t)(int argc, char **argv);
int noza_process_exec(main_t entry, int argc, char *argv[], int *exit_code);
int noza_process_exec_with_stack(main_t entry, int argc, char *argv[], int *exit_code, uint32_t stack_size);
int noza_process_exec_detached(main_t entry, int argc, char *argv[]);
int noza_process_exec_detached_with_stack(main_t entry, int argc, char *argv[], uint32_t stack_size);
