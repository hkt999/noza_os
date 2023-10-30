#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nozaos.h"
#include "string_map.h"
#include "name_lookup_server.h"
#include "name_lookup_client.h"

typedef struct {
    value_node_t node_slot[NOZA_MAX_SERVICE];
    value_node_t *free_head;
} node_mgr_t;

static value_node_t *node_alloc(void *p) {
    value_node_t *r;
    node_mgr_t *m = (node_mgr_t *)p;
    if (m->free_head) {
        r = m->free_head;
        m->free_head = m->free_head->mlink;
    }
    return r;
}

static void node_free(value_node_t *node, void *p) {
    node_mgr_t *m = (node_mgr_t *)p;
    node->mlink = m->free_head;
    m->free_head = node;
}

static void node_mgr_init(node_mgr_t *m) {
    memset(m, 0, sizeof(node_mgr_t));
    for (int i=0; i<NOZA_MAX_SERVICE-1; i++) {
        m->node_slot[i].mlink = &m->node_slot[i+1];
    }
    m->node_slot[NOZA_MAX_SERVICE-1].mlink = NULL; // last one
    m->free_head = &m->node_slot[0];
}

static void map_init(simap_t *m, node_mgr_t *node_mgr) {
    memset(m, 0, sizeof(simap_t));
    m->root = NULL;
    m->node_alloc = node_alloc;
    m->node_free = node_free;
    m->alloc_param = node_mgr;
}

static int do_name_server(void *param, uint32_t pid)
{
    noza_msg_t msg;
    simap_t map;
    static node_mgr_t node_mgr;

    if (pid != NAME_SERVER_PID) {
        printf("name server panic: invalid pid %d\n", pid);
        return -1;
    }

    // setup node manager
    node_mgr_init(&node_mgr);
    map_init(&map, &node_mgr);
    simap_init(&map, node_alloc, node_free, &node_mgr);
    for (;;) {
        if (noza_recv(&msg) == 0) {
            name_msg_t *name_msg = (name_msg_t *)msg.ptr;
            switch (name_msg->cmd) {
                case NAME_LOOKUP_REGISTER:
                    name_msg->code = simap_set_value(&map, name_msg->name, name_msg->value);
                    break;

                case NAME_LOOKUP_SEARCH:
                    name_msg->code = simap_get_value(&map, name_msg->name, (int *)&name_msg->value);
                    break;

                case NAME_LOOKUP_UNREGISTER:
                    name_msg->code = simap_clear(&map, name_msg->name);
                    break;

                default:
                    name_msg->code = -1;
                    break;
            }
            noza_reply(&msg);
        }
    }

    return 0;
}

static uint8_t name_server_stack[256]; // TODO: reconsider the stack size
void __attribute__((constructor(101))) name_server_init(void *param, uint32_t pid)
{
    // TODO: move the external declaraction into a header file
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
	noza_add_service(do_name_server, name_server_stack, sizeof(name_server_stack)); // TODO: add stack size
}