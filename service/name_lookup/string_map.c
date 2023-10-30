#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "posix/errno.h"
#include "string_map.h"

static value_node_t *new_node(const char *key, int value, node_alloc_t node_alloc, void *alloc_param) {
    value_node_t *node = node_alloc(alloc_param);
    if (node) {
        strncpy(node->key, key, MAX_NAME_LEN); // copy key 
        node->value = value;
        node->left = node->right = NULL;
        node->height = 1;
    }
    return node;
}

static int get_height(value_node_t *N) {
    if (N == NULL) return 0;
    return N->height;
}

static int get_balance(value_node_t *N) {
    if (N == NULL) return 0;
    return get_height(N->left) - get_height(N->right);
}

static value_node_t *right_rotate(value_node_t *y) {
    value_node_t *x = y->left;
    value_node_t *T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = 1 + (get_height(y->left) > get_height(y->right) ? get_height(y->left) : get_height(y->right));
    x->height = 1 + (get_height(x->left) > get_height(x->right) ? get_height(x->left) : get_height(x->right));
    return x;
}

static value_node_t *left_rotate(value_node_t *x) {
    value_node_t *y = x->right;
    value_node_t *T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = 1 + (get_height(x->left) > get_height(x->right) ? get_height(x->left) : get_height(x->right));
    y->height = 1 + (get_height(y->left) > get_height(y->right) ? get_height(y->left) : get_height(y->right));
    return y;
}

static value_node_t *insert_value(value_node_t *node, const char *key, int value, node_alloc_t node_alloc, void *alloc_param) {
    if (node == NULL) {
        return new_node(key, value, node_alloc, alloc_param);
    }

    // TODO: check if the value is duplicated or not
    int cmp_key = strncmp(key, node->key, MAX_NAME_LEN);
    if (cmp_key < 0)
        node->left = insert_value(node->left, key, value, node_alloc, alloc_param);
    else if (cmp_key > 0)
        node->right = insert_value(node->right, key, value, node_alloc, alloc_param);
    else
        return node;

    node->height = 1 + (get_height(node->left) > get_height(node->right) ? get_height(node->left) : get_height(node->right));

    // balance
    int balance = get_balance(node);
    int cmp_key_left = strncmp(key, node->left->key, MAX_NAME_LEN);
    if (balance > 1 && cmp_key_left < 0) {
        return right_rotate(node);
    }
    int cmp_key_right = strncmp(key, node->right->key, MAX_NAME_LEN);
    if (balance < -1 && cmp_key_right > 0) {
        return left_rotate(node);
    }
    if (balance > 1 && cmp_key_left > 0) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }
    if (balance < -1 && cmp_key_right < 0) {
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    return node;
}

static value_node_t *min_value_node(value_node_t* node) {
    value_node_t *current = node;
    while (current->left != NULL) current = current->left;
    return current;
}

static value_node_t *remove_value(value_node_t* root, const char *key, node_free_t node_free, void *alloc_param) {
    if (root == NULL) return root;

    int cmp_key_root = strncmp(key, root->key, MAX_NAME_LEN);
    if (cmp_key_root < 0) {
        root->left = remove_value(root->left, key, node_free, alloc_param);
    } else if (cmp_key_root > 0) {
        root->right = remove_value(root->right, key, node_free, alloc_param);
    } else {
        if ((root->left == NULL) || (root->right == NULL)) {
            value_node_t *temp = root->left ? root->left : root->right;

            if (temp == NULL) {
                temp = root;
                root = NULL;
            } else *root = *temp;

            node_free(temp, alloc_param);
        } else {
            value_node_t *temp = min_value_node(root->right);
            strncpy(root->key, temp->key, MAX_NAME_LEN);
            root->right = remove_value(root->right, temp->key, node_free, alloc_param);
        }
    }

    if (root == NULL)
        return root;

    root->height = 1 + (get_height(root->left) > get_height(root->right) ? get_height(root->left) : get_height(root->right));

    int balance = get_balance(root);

    if (balance > 1 && get_balance(root->left) >= 0) return right_rotate(root);
    if (balance > 1 && get_balance(root->left) < 0) {
        root->left = left_rotate(root->left);
        return right_rotate(root);
    }
    if (balance < -1 && get_balance(root->right) <= 0) return left_rotate(root);
    if (balance < -1 && get_balance(root->right) > 0) {
        root->right = right_rotate(root->right);
        return left_rotate(root);
    }

    return root;
}

void simap_init(simap_t *map, node_alloc_t node_alloc, node_free_t node_free, void *alloc_param) {
    map->root = NULL;
    map->node_alloc = node_alloc;
    map->node_free = node_free;
    map->alloc_param = alloc_param;
}

int simap_get_value(simap_t *map, const char *key, int *value) {
    value_node_t *root = map->root;
    while (root != NULL) {
        if (strncmp(key, root->key, MAX_NAME_LEN) < 0)
            root = root->left;
        else if (strncmp(key, root->key, MAX_NAME_LEN) > 0)
            root = root->right;
        else {
            if (value) *value = root->value;
            return 0; // success
        }
    }
    return ENOENT; // not found
}

int simap_set_value(simap_t *map, const char *key, int value) {
    // check if the slot is available or not
    value_node_t *test_node = map->node_alloc(map->alloc_param);
    if (test_node == NULL)
        return EAGAIN; // fail

    map->node_free(test_node, map->alloc_param);

    if (simap_get_value(map, key, NULL) == 0)
        return EBUSY; // fail (duplicated)

    map->root = insert_value(map->root, key, value, map->node_alloc, map->alloc_param);
    return 0;
}

int simap_clear(simap_t *map, const char *key) {
    if (simap_get_value(map, key, NULL) != 0)
        return ENOENT; // not found

    map->root = remove_value(map->root, key, map->node_free, map->alloc_param);
    return 0;
}

const char *simap_min_key(simap_t *map) {
    value_node_t *current = map->root;
    while (current->left != NULL) {
        current = current->left;
    }
    return current->key;
}

const char *simap_max_key(simap_t *map) {
    value_node_t *current = map->root;
    while (current->right != NULL) {
        current = current->right;
    }
    return current->key;
}
