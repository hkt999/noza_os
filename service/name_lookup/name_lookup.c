#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nozaos.h"

#define NOZA_MAX_SERVICE    16
#define MAX_NAME_LEN        16

typedef struct {
    char name[MAX_NAME_LEN]; // name
    uint32_t pid; // service id
} name_lookup_msg_t;

static name_lookup_msg_t name_lookup_table[NOZA_MAX_SERVICE];

#define NAME_LOOKUP_REGISTER    1
#define NAME_LOOKUP_LOOKUP      2
#define NAME_LOOKUP_UNREGISTER  3

typedef struct name_msg_s {
    union {
        uint32_t cmd;
        int32_t reply;
    };
    const char *name;
    uint32_t pid;
} name_msg_t;

extern void __user_start(void *param, uint32_t pid);
static void do_name_lookup(void *param, uint32_t pid)
{
    noza_msg_t msg;
    memset(name_lookup_table, 0, sizeof(name_lookup_table));
    noza_thread_create(__user_start, NULL, 0);
    for (;;) {
        if (noza_recv(&msg) == 0) {
            name_msg_t *name_msg = (name_msg_t *)msg.ptr;
            switch (name_msg->cmd) {
                case NAME_LOOKUP_REGISTER:
                    for (int i = 0; i < NOZA_MAX_SERVICE; i++) {
                        if (name_lookup_table[i].pid == 0) {
                            name_lookup_table[i].pid = name_msg->pid;
                            strncpy(name_lookup_table[i].name, name_msg->name, MAX_NAME_LEN);
                            name_msg->reply = 0; // success
                            noza_reply(&msg);
                            return;
                        }
                    }
                    name_msg->reply = -1;
                    noza_reply(&msg);
                    break;

                case NAME_LOOKUP_LOOKUP:
                    for (int i = 0; i < NOZA_MAX_SERVICE; i++) {
                        if (strncmp(name_lookup_table[i].name, name_msg->name, MAX_NAME_LEN) == 0) {
                            name_msg->reply = 0; // success
                            name_msg->pid = name_lookup_table[i].pid;
                            noza_reply(&msg);
                            return;
                        }
                    }
                    name_msg->reply = -2;
                    noza_reply(&msg);
                    break;

                case NAME_LOOKUP_UNREGISTER:
                    for (int i = 0; i < NOZA_MAX_SERVICE; i++) {
                        if (name_lookup_table[i].pid == name_msg->pid) {
                            name_lookup_table[i].pid = 0;
                            name_msg->reply = 0;
                            noza_reply(&msg); // success
                            return;
                        }
                    }
                    name_msg->reply = -3;
                    noza_reply(&msg);
                    break;

                default:
                    name_msg->reply = -4;
                    noza_reply(&msg);
                    break;
            }
        }
    }
}

typedef struct {
	void (*user_entry)(void *param, uint32_t pid);
	void *user_param;
	uint32_t *stack_ptr;
	uint32_t stack_size;
	uint32_t created;
} boot_info_t;

extern void app_bootstrap(void *param, uint32_t pid);

void name_lookup_init(void *param, uint32_t pid)
{
	boot_info_t boot_info;
	boot_info.user_entry = do_name_lookup;
	boot_info.user_param = param;
	boot_info.stack_ptr = (uint32_t *)malloc(1024); // TODO: use noza memory allocator
	boot_info.stack_size = 1024;
	boot_info.created = 0;
    app_bootstrap(&boot_info, pid);
}


// the client api
int name_lookup_register(const char *name, uint32_t pid)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_REGISTER, .name = name, .pid = 0};
    noza_msg_t noza_msg = {.pid = 0, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);
    return msg.reply;
}

int name_lookup_lookup(const char *name, uint32_t *pid)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_LOOKUP, .name = name};
    noza_msg_t noza_msg = {.pid = 0, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);
    if (msg.reply == 0) {
        *pid = msg.pid;
    }
    return msg.reply;
}

int name_lookup_unregister(uint32_t pid)
{
    name_msg_t msg = {.cmd = NAME_LOOKUP_UNREGISTER, .pid = pid};
    noza_msg_t noza_msg = {.pid = 0, .ptr = (void *)&msg, .size = sizeof(msg)};
    noza_call(&noza_msg);
    return msg.reply;
}
