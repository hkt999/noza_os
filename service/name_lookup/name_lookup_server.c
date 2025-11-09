#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nozaos.h"
#include "string_map.h"
#include "name_lookup_server.h"
#include "posix/errno.h"

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

typedef struct {
    uint32_t service_id;
    uint32_t vid;
    char name[MAX_NAME_LEN+1];
    uint8_t in_use;
} service_binding_t;

static service_binding_t service_table[NOZA_MAX_SERVICE];
static uint32_t next_service_id = 1;

static service_binding_t *binding_alloc(void)
{
    for (int i = 0; i < NOZA_MAX_SERVICE; i++) {
        if (!service_table[i].in_use) {
            service_table[i].in_use = 1;
            service_table[i].service_id = 0;
            service_table[i].vid = 0;
            service_table[i].name[0] = '\0';
            return &service_table[i];
        }
    }
    return NULL;
}

static service_binding_t *binding_by_id(uint32_t service_id)
{
    if (service_id == 0) {
        return NULL;
    }
    for (int i = 0; i < NOZA_MAX_SERVICE; i++) {
        if (service_table[i].in_use && service_table[i].service_id == service_id) {
            return &service_table[i];
        }
    }
    return NULL;
}

static void binding_release(service_binding_t *binding)
{
    if (binding == NULL) {
        return;
    }
    binding->in_use = 0;
    binding->service_id = 0;
    binding->vid = 0;
    binding->name[0] = '\0';
}

static uint32_t allocate_service_id(void)
{
    for (int retry = 0; retry < NOZA_MAX_SERVICE * 4; retry++) {
        uint32_t candidate = next_service_id++;
        if (next_service_id == 0) {
            next_service_id = 1;
        }
        if (binding_by_id(candidate) == NULL) {
            return candidate;
        }
    }
    return 0;
}

static int validate_name(const char *name)
{
    if (name == NULL) {
        return 0;
    }
    size_t len = strnlen(name, MAX_NAME_LEN + 1);
    if (len == 0 || len > MAX_NAME_LEN) {
        return 0;
    }
    return 1;
}

static int handle_register(name_msg_t *name_msg, uint32_t caller_vid, simap_t *map)
{
    if (!validate_name(name_msg->name)) {
        return NAME_LOOKUP_ERR_INVALID;
    }

    int stored_id;
    if (simap_get_value(map, name_msg->name, &stored_id) == 0) {
        service_binding_t *binding = binding_by_id((uint32_t)stored_id);
        if (binding == NULL) {
            return NAME_LOOKUP_ERR_INVALID;
        }
        if (name_msg->service_id != 0 && name_msg->service_id != binding->service_id) {
            return NAME_LOOKUP_ERR_DUPLICATE;
        }
        binding->vid = caller_vid;
        name_msg->service_id = binding->service_id;
        return NAME_LOOKUP_OK;
    }

    if (name_msg->service_id != 0 && binding_by_id(name_msg->service_id) != NULL) {
        return NAME_LOOKUP_ERR_DUPLICATE;
    }

    service_binding_t *binding = binding_alloc();
    if (binding == NULL) {
        return NAME_LOOKUP_ERR_CAPACITY;
    }

    if (name_msg->service_id == 0) {
        uint32_t generated = allocate_service_id();
        if (generated == 0) {
            binding_release(binding);
            return NAME_LOOKUP_ERR_CAPACITY;
        }
        name_msg->service_id = generated;
    }

    binding->service_id = name_msg->service_id;
    binding->vid = caller_vid;
    strncpy(binding->name, name_msg->name, MAX_NAME_LEN);
    binding->name[MAX_NAME_LEN] = '\0';

    int ret = simap_set_value(map, binding->name, (int)binding->service_id);
    if (ret != 0) {
        binding_release(binding);
        if (ret == EBUSY) {
            return NAME_LOOKUP_ERR_DUPLICATE;
        }
        return NAME_LOOKUP_ERR_CAPACITY;
    }

    return NAME_LOOKUP_OK;
}

static int handle_resolve_name(name_msg_t *name_msg, simap_t *map)
{
    if (!validate_name(name_msg->name)) {
        return NAME_LOOKUP_ERR_INVALID;
    }

    int stored_id;
    if (simap_get_value(map, name_msg->name, &stored_id) != 0) {
        return NAME_LOOKUP_ERR_NOT_FOUND;
    }

    service_binding_t *binding = binding_by_id((uint32_t)stored_id);
    if (binding == NULL) {
        return NAME_LOOKUP_ERR_NOT_FOUND;
    }

    name_msg->service_id = binding->service_id;
    name_msg->vid = binding->vid;
    return NAME_LOOKUP_OK;
}

static int handle_resolve_id(name_msg_t *name_msg)
{
    service_binding_t *binding = binding_by_id(name_msg->service_id);
    if (binding == NULL) {
        return NAME_LOOKUP_ERR_NOT_FOUND;
    }
    name_msg->vid = binding->vid;
    return NAME_LOOKUP_OK;
}

static int handle_unregister(name_msg_t *name_msg, simap_t *map)
{
    service_binding_t *binding = NULL;
    if (name_msg->service_id != 0) {
        binding = binding_by_id(name_msg->service_id);
    } else if (validate_name(name_msg->name)) {
        int stored_id;
        if (simap_get_value(map, name_msg->name, &stored_id) == 0) {
            binding = binding_by_id((uint32_t)stored_id);
        }
    }

    if (binding == NULL) {
        return NAME_LOOKUP_ERR_NOT_FOUND;
    }

    simap_clear(map, binding->name);
    binding_release(binding);
    return NAME_LOOKUP_OK;
}

static int do_name_server(void *param, uint32_t pid)
{
    noza_msg_t msg;
    simap_t map;
    static node_mgr_t node_mgr;

    (void)param;
    (void)pid;

    noza_thread_bind_vid(NAME_SERVER_VID);

    // setup node manager
    node_mgr_init(&node_mgr);
    map_init(&map, &node_mgr);
    simap_init(&map, node_alloc, node_free, &node_mgr);
    for (;;) {
        if (noza_recv(&msg) == 0) {
            name_msg_t *name_msg = (name_msg_t *)msg.ptr;
            switch (name_msg->cmd) {
                case NAME_LOOKUP_REGISTER:
                    name_msg->code = handle_register(name_msg, msg.to_vid, &map);
                    break;

                case NAME_LOOKUP_RESOLVE_NAME:
                    name_msg->code = handle_resolve_name(name_msg, &map);
                    break;

                case NAME_LOOKUP_RESOLVE_ID:
                    name_msg->code = handle_resolve_id(name_msg);
                    break;

                case NAME_LOOKUP_UNREGISTER:
                    name_msg->code = handle_unregister(name_msg, &map);
                    break;

                default:
                    name_msg->code = NAME_LOOKUP_ERR_INVALID;
                    break;
            }
            noza_reply(&msg);
        }
    }

    return 0;
}

static uint8_t name_server_stack[1024];
void __attribute__((constructor(101))) name_server_init(void *param, uint32_t pid)
{
    // TODO: move the external declaraction into a header file
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
	noza_add_service(do_name_server, name_server_stack, sizeof(name_server_stack)); // TODO: add stack size
}
