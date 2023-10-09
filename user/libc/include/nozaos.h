// NozaOS
#pragma once

#include <stdint.h>

// Noza IPC
typedef struct {
    uint32_t    pid;
    void        *ptr;
    uint32_t    size;
} noza_msg_t;

#define NO_AUTO_FREE_STACK	0
#define AUTO_FREE_STACK	1

// Noza thread & scheduling
int     noza_thread_sleep_us(int64_t us, int64_t *remain_us);
int     noza_thread_sleep_ms(int64_t ms, int64_t *remain_ms);
int     noza_thread_create(uint32_t *pth, int (*entry)(void *param, uint32_t pid), void *param, uint32_t priority, uint32_t stack_size);
int     noza_thread_create_with_stack(uint32_t *pth, int (*entry)(void *param, uint32_t pid), void *param, uint32_t priority, void *stack_addr, uint32_t stack_size, uint32_t auto_free);
int     noza_thread_change_priority(uint32_t thread_id, uint32_t priority);
int     noza_thread_yield();
int     noza_thread_detach(uint32_t thread_id);
int     noza_thread_join(uint32_t thread_id, uint32_t *code);
void    noza_thread_terminate(int exit_code);
int     noza_thread_self(uint32_t *pid);

// NOza message
int noza_recv(noza_msg_t *msg);
int noza_reply(noza_msg_t *msg);
int noza_call(noza_msg_t *msg);
int noza_nonblock_call(noza_msg_t *msg);
int noza_nonblock_recv(noza_msg_t *msg);
