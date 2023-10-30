#pragma once

#define MAX_NAME_LEN        16

typedef struct value_node_s {
    char key[MAX_NAME_LEN+1];
    int value;
    int height;
    struct value_node_s* left;
    struct value_node_s* right;
    struct value_node_s* mlink;
} value_node_t;

typedef value_node_t* (*node_alloc_t)(void *p);
typedef void (*node_free_t)(value_node_t *node, void *p);

// exported interface
typedef struct {
    value_node_t *root;
    node_alloc_t node_alloc;
    node_free_t node_free;
    void *alloc_param;
} simap_t;

void simap_init(simap_t *map, node_alloc_t node_alloc, node_free_t node_free, void *alloc_param);
int simap_get_value(simap_t *map, const char *key, int *value);
int simap_set_value(simap_t *map, const char *key, int value);
int simap_clear(simap_t *map, const char *key);
const char *simap_min_key(simap_t *map);
const char *simap_max_key(simap_t *map);