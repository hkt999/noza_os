#pragma once
#include <stdint.h>

// Noza thread & scheduling
int     noza_thread_sleep(uint32_t ms);
int     noza_thread_create(void (*entry)(void *param), void *param, uint32_t priority);
int     noza_thread_change_priority(uint32_t thread_id, uint32_t priority);
int     noza_thread_yield();
void    noza_thread_terminate();

// Noza IPC
typedef struct {
    uint32_t pid;
} noza_port_t;

typedef struct {
    uint8_t     *ptr;
    uint32_t    size;
} noza_msg_t;

int noza_recv(noza_port_t *port, noza_msg_t *msg);
int noza_reply(noza_port_t *port, uint32_t reply_code);
int noza_call(noza_port_t *port, noza_msg_t *msg);
int noza_nonblock_call(noza_port_t *port, noza_msg_t *msg);
int noza_nonblock_recv(noza_port_t *port, noza_msg_t *msg);
