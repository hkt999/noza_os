#include <stdlib.h>
#include "printk.h"

typedef struct value_node {
    int key;
    int value;
    int height;
    struct value_node* left;
    struct value_node* right;
} value_node;

static value_node* new_node(int key, int value) {
    value_node* node = (value_node*)malloc(sizeof(value_node));
    node->key = key;
    node->value = value;
    node->left = node->right = NULL;
    node->height = 1;
    return node;
}

static int get_height(value_node* N) {
    if (N == NULL) return 0;
    return N->height;
}

static int get_balance(value_node* N) {
    if (N == NULL) return 0;
    return get_height(N->left) - get_height(N->right);
}

static value_node* right_rotate(value_node* y) {
    value_node* x = y->left;
    value_node* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = 1 + (get_height(y->left) > get_height(y->right) ? get_height(y->left) : get_height(y->right));
    x->height = 1 + (get_height(x->left) > get_height(x->right) ? get_height(x->left) : get_height(x->right));
    return x;
}

static value_node* left_rotate(value_node* x) {
    value_node* y = x->right;
    value_node* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = 1 + (get_height(x->left) > get_height(x->right) ? get_height(x->left) : get_height(x->right));
    y->height = 1 + (get_height(y->left) > get_height(y->right) ? get_height(y->left) : get_height(y->right));
    return y;
}

static value_node* insert_value(value_node* node, int key, int value) {
    if (node == NULL) return new_node(key, value);
    if (key < node->key) node->left = insert_value(node->left, key, value);
    else if (key > node->key) node->right = insert_value(node->right, key, value);
    else return node;

    node->height = 1 + (get_height(node->left) > get_height(node->right) ? get_height(node->left) : get_height(node->right));

    int balance = get_balance(node);

    if (balance > 1 && key < node->left->key) return right_rotate(node);
    if (balance < -1 && key > node->right->key) return left_rotate(node);
    if (balance > 1 && key > node->left->key) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }
    if (balance < -1 && key < node->right->key) {
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    return node;
}

static value_node* min_value_node(value_node* node) {
    value_node* current = node;
    while (current->left != NULL) current = current->left;
    return current;
}

static value_node* remove_value(value_node* root, int key) {
    if (root == NULL) return root;

    if (key < root->key) root->left = remove_value(root->left, key);
    else if (key > root->key) root->right = remove_value(root->right, key);
    else {
        if ((root->left == NULL) || (root->right == NULL)) {
            value_node* temp = root->left ? root->left : root->right;

            if (temp == NULL) {
                temp = root;
                root = NULL;
            } else *root = *temp;

            free(temp);
        } else {
            value_node* temp = min_value_node(root->right);
            root->key = temp->key;
            root->right = remove_value(root->right, temp->key);
        }
    }

    if (root == NULL) return root;

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

int get_value(value_node* root, int key) {
    while (root != NULL) {
        if (key < root->key) root = root->left;
        else if (key > root->key) root = root->right;
        else return root->value;
    }
    return -1; // Key not found
}

int min_key(value_node* root) {
    value_node* current = root;
    while (current->left != NULL) {
        current = current->left;
    }
    return current->key;
}

int max_key(value_node* root) {
    value_node* current = root;
    while (current->right != NULL) {
        current = current->right;
    }
    return current->key;
}

#if 0
int main() {
    value_node* root = NULL;
    root = insert_value(root, 10, 100);
    root = insert_value(root, 20, 200);
    root = insert_value(root, 30, 300);
    printk("%d\n", get_value(root, 10)); // Outputs: 100
    printk("%d\n", get_value(root, 20)); // Outputs: 200

    root = remove_value(root, 20);
    printk("%d\n", get_value(root, 20)); // Outputs: -1 (since it's removed)

    return 0;
}
#endif
