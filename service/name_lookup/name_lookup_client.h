#pragma once

#include "name_lookup_client.h"
#include "name_lookup_server.h"
#define ROOT_PID 0

// the client api
int name_lookup_register(const char *name, uint32_t pid)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_REGISTER, .name = name, .pid = 0};
    noza_msg_t noza_msg = {.to_pid = ROOT_PID, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);
    return msg.reply;
}

int name_lookup_lookup(const char *name, uint32_t *pid)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_LOOKUP, .name = name};
    noza_msg_t noza_msg = {.to_pid = ROOT_PID, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);
    if (msg.reply == 0) {
        *pid = msg.pid;
    }
    return msg.reply;
}

int name_lookup_unregister(uint32_t pid)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_UNREGISTER, .pid = pid};
    noza_msg_t noza_msg = {.to_pid = ROOT_PID, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);
    return msg.reply;
}

// root service for naming
static uint8_t name_lookup_stack[1024];
void __attribute__((constructor(101))) name_lookup_init(void *param, uint32_t pid)
{
    // TODO: move the external declaraction into a header file
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
    noza_add_service(do_name_lookup, name_lookup_stack, sizeof(name_lookup_stack));
}