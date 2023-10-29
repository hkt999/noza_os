#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nozaos.h"
#include "name_lookup_server.h"

#define NOZA_MAX_SERVICE    16
#define MAX_NAME_LEN        16

typedef struct {
    char name[MAX_NAME_LEN]; // name
    uint32_t value;
} name_lookup_msg_t;

static name_lookup_msg_t name_lookup_table[NOZA_MAX_SERVICE];

static int find_empty()
{
    for (int i = 0; i < NOZA_MAX_SERVICE; i++) {
        if (name_lookup_table[i].value == 0) {
            return i;
        }
    }
    return -1;
}

static int find_name(const char *name)
{
    for (int i = 0; i < NOZA_MAX_SERVICE; i++) {
        if (strncmp(name_lookup_table[i].name, name, MAX_NAME_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

static int do_name_lookup(void *param, uint32_t pid)
{
    int idx;
    noza_msg_t msg;
    memset(name_lookup_table, 0, sizeof(name_lookup_table));
    for (;;) {
        if (noza_recv(&msg) == 0) {
            name_msg_t *name_msg = (name_msg_t *)msg.ptr;
            switch (name_msg->cmd) {
                case NAME_LOOKUP_REGISTER:
                    if ((idx = find_empty()) >= 0) {
                        strncpy(name_lookup_table[idx].name, name_msg->name, MAX_NAME_LEN);
                        name_lookup_table[idx].value = name_msg->value;
                        name_msg->code = 0;
                    } else {
                        name_msg->code = -1;
                    }
                    break;

                case NAME_LOOKUP_LOOKUP:
                    if ((idx = find_name(name_msg->name)) >= 0) {
                        name_msg->value = name_lookup_table[idx].value;
                        name_msg->code = 0;
                    } else {
                        name_msg->code = -2;
                    }
                    break;

                case NAME_LOOKUP_UNREGISTER:
                    if ((idx = find_name(name_msg->name)) >= 0) {
                        name_lookup_table[idx].value = 0;
                        name_msg->code = 0;
                    } else {
                        name_msg->code = -3;
                    }
                    break;

                default:
                    name_msg->code = -4;
                    break;
            }
            noza_reply(&msg);
        }
    }

    return 0;
}
