// NozaOS
#pragma once

#include <stdint.h>

// Noza IPC
typedef struct {
    uint32_t    pid;
    void        *ptr;
    uint32_t    size;
} noza_msg_t;

// Noza thread & scheduling
int     noza_thread_sleep(uint32_t ms);
int     noza_thread_create(int (*entry)(void *param, uint32_t pid), void *param, uint32_t priority);
int     noza_thread_change_priority(uint32_t thread_id, uint32_t priority);
int     noza_thread_yield();
int     noza_thread_detach(uint32_t thread_id);
int     noza_thread_join(uint32_t thread_id, uint32_t *code);
void    noza_thread_terminate(int exit_code);

// NOza message
int noza_recv(noza_msg_t *msg);
int noza_reply(noza_msg_t *msg);
int noza_call(noza_msg_t *msg);
int noza_nonblock_call(noza_msg_t *msg);
int noza_nonblock_recv(noza_msg_t *msg);
