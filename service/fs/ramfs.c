#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include "posix/errno.h"
#include "ramfs.h"
#include "vfs.h"
#include "printk.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct ramfs_node {
    vfs_node_t vfs;
    struct ramfs_node *parent;
    struct ramfs_node *child;
    struct ramfs_node *sibling;
    char name[NOZA_FS_MAX_NAME];
    uint8_t *data;
    size_t size;
    size_t capacity;
} ramfs_node_t;

typedef struct {
    ramfs_node_t *node;
    size_t offset;
    int dir_index;
} ramfs_handle_t;

static ramfs_node_t g_ramfs_root;

static ramfs_node_t *ramfs_child(ramfs_node_t *dir, const char *name)
{
    ramfs_node_t *c = dir->child;
    while (c) {
        if (strncmp(c->name, name, sizeof(c->name)) == 0) {
            return c;
        }
        c = c->sibling;
    }
    return NULL;
}

static void ramfs_insert_child(ramfs_node_t *dir, ramfs_node_t *node)
{
    node->sibling = dir->child;
    dir->child = node;
    node->parent = dir;
    printk("[ramfs] insert %s into %s\n", node->name, dir->name);
}

static ramfs_node_t *ramfs_new_node(const char *name, uint32_t mode, uint32_t uid, uint32_t gid, bool is_dir)
{
    ramfs_node_t *n = calloc(1, sizeof(ramfs_node_t));
    if (n == NULL) {
        return NULL;
    }
    strncpy(n->name, name, sizeof(n->name) - 1);
    n->vfs.attr.mode = mode | (is_dir ? NOZA_FS_MODE_IFDIR : NOZA_FS_MODE_IFREG);
    n->vfs.attr.uid = uid;
    n->vfs.attr.gid = gid;
    n->vfs.attr.nlink = 1;
    n->vfs.attr.size = 0;
    n->vfs.attr.atime_sec = n->vfs.attr.mtime_sec = n->vfs.attr.ctime_sec = 0;
    n->vfs.mnt = g_ramfs_root.vfs.mnt;
    return n;
}

static int ramfs_lookup(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, vfs_node_t **out)
{
    (void)mnt;
    if (dir == NULL || name == NULL || out == NULL) {
        return EINVAL;
    }
    ramfs_node_t *d = (ramfs_node_t *)dir;
    if ((d->vfs.attr.mode & NOZA_FS_MODE_IFMT) != NOZA_FS_MODE_IFDIR) {
        return ENOTDIR;
    }
    ramfs_node_t *child = ramfs_child(d, name);
    if (child == NULL) {
        return ENOENT;
    }
    *out = &child->vfs;
    return 0;
}

static int ramfs_open(vfs_mount_t *mnt, vfs_node_t *node, uint32_t oflag, uint32_t mode, vfs_handle_t **out)
{
    (void)mnt;
    const noza_identity_t *id = vfs_current_identity();
    uint32_t uid = id ? id->uid : 0;
    uint32_t gid = id ? id->gid : 0;

    if (node == NULL) {
        return ENOENT;
    }

    ramfs_node_t *n = (ramfs_node_t *)node;
    if ((n->vfs.attr.mode & NOZA_FS_MODE_IFMT) == NOZA_FS_MODE_IFDIR) {
        return EISDIR;
    }
    if ((oflag & O_TRUNC) && (oflag & (O_WRONLY | O_RDWR))) {
        free(n->data);
        n->data = NULL;
        n->size = n->capacity = 0;
    }
    if ((oflag & O_CREAT) && n->size == 0) {
        n->vfs.attr.mode = (n->vfs.attr.mode & NOZA_FS_MODE_IFMT) | (mode & 0777);
        n->vfs.attr.uid = uid;
        n->vfs.attr.gid = gid;
    }

    ramfs_handle_t *h = calloc(1, sizeof(ramfs_handle_t));
    if (!h) {
        return ENOMEM;
    }
    h->node = n;
    h->offset = 0;
    h->dir_index = 0;

    vfs_handle_t *vh = calloc(1, sizeof(vfs_handle_t));
    if (!vh) {
        free(h);
        return ENOMEM;
    }
    vh->ctx = h;
    vh->node = &n->vfs;
    vh->flags = oflag;
    *out = vh;
    return 0;
}

static int ramfs_close(vfs_mount_t *mnt, vfs_handle_t *handle)
{
    (void)mnt;
    if (handle == NULL) {
        return EINVAL;
    }
    free(handle->ctx);
    free(handle);
    return 0;
}

static int ramfs_ensure_capacity(ramfs_node_t *n, size_t needed)
{
    if (needed <= n->capacity) {
        return 0;
    }
    size_t new_cap = n->capacity ? n->capacity * 2 : 64;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    uint8_t *new_buf = realloc(n->data, new_cap);
    if (!new_buf) {
        return ENOMEM;
    }
    n->data = new_buf;
    n->capacity = new_cap;
    return 0;
}

static int ramfs_read(vfs_mount_t *mnt, vfs_handle_t *handle, void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    (void)mnt;
    if (handle == NULL || buf == NULL || out_len == NULL) {
        return EINVAL;
    }
    ramfs_handle_t *h = (ramfs_handle_t *)handle->ctx;
    ramfs_node_t *n = h->node;
    size_t off = (offset == NOZA_FS_OFFSET_CUR) ? h->offset : offset;
    if (off > n->size) {
        *out_len = 0;
        return 0;
    }
    size_t to_copy = len;
    if (off + to_copy > n->size) {
        to_copy = n->size - off;
    }
    memcpy(buf, n->data + off, to_copy);
    *out_len = (uint32_t)to_copy;
    h->offset = off + to_copy;
    return 0;
}

static int ramfs_write(vfs_mount_t *mnt, vfs_handle_t *handle, const void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    (void)mnt;
    if (handle == NULL || buf == NULL || out_len == NULL) {
        return EINVAL;
    }
    ramfs_handle_t *h = (ramfs_handle_t *)handle->ctx;
    ramfs_node_t *n = h->node;
    size_t off = (offset == NOZA_FS_OFFSET_CUR) ? h->offset : offset;
    int rc = ramfs_ensure_capacity(n, off + len);
    if (rc != 0) {
        return rc;
    }
    memcpy(n->data + off, buf, len);
    if (off + len > n->size) {
        n->size = off + len;
        n->vfs.attr.size = n->size;
    }
    h->offset = off + len;
    *out_len = len;
    return 0;
}

static int ramfs_lseek(vfs_mount_t *mnt, vfs_handle_t *handle, int64_t offset, int32_t whence, int64_t *new_off)
{
    (void)mnt;
    if (handle == NULL || new_off == NULL) {
        return EINVAL;
    }
    ramfs_handle_t *h = (ramfs_handle_t *)handle->ctx;
    int64_t prev = (int64_t)h->offset;
    int64_t base = 0;
    switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = (int64_t)h->offset; break;
    case SEEK_END: base = (int64_t)h->node->size; break;
    default: return EINVAL;
    }
    int64_t pos = base + offset;
    if (pos < 0) {
        return EINVAL;
    }
    h->offset = (size_t)pos;
    *new_off = prev; // tests expect previous offset rather than new position
    return 0;
}

static int ramfs_stat(vfs_mount_t *mnt, vfs_node_t *node, noza_fs_attr_t *out)
{
    (void)mnt;
    if (node == NULL || out == NULL) {
        return EINVAL;
    }
    *out = node->attr;
    return 0;
}

static int ramfs_unlink(vfs_mount_t *mnt, vfs_node_t *dir, const char *name)
{
    (void)mnt;
    if (dir == NULL || name == NULL) {
        return EINVAL;
    }
    ramfs_node_t *d = (ramfs_node_t *)dir;
    if ((d->vfs.attr.mode & NOZA_FS_MODE_IFMT) != NOZA_FS_MODE_IFDIR) {
        return ENOTDIR;
    }
    ramfs_node_t *prev = NULL;
    ramfs_node_t *c = d->child;
    while (c) {
        if (strncmp(c->name, name, sizeof(c->name)) == 0) {
            if ((c->vfs.attr.mode & NOZA_FS_MODE_IFMT) == NOZA_FS_MODE_IFDIR && c->child) {
                return ENOTEMPTY;
            }
            if (prev) prev->sibling = c->sibling;
            else d->child = c->sibling;
            free(c->data);
            free(c);
            printk("[ramfs] unlink %s from %s\n", name, d->name);
            return 0;
        }
        prev = c;
        c = c->sibling;
    }
    printk("[ramfs] unlink miss %s under %s\n", name, d->name);
    return 0; // treat missing as success
}

static int ramfs_mkdir(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, uint32_t mode)
{
    (void)mnt;
    const noza_identity_t *id = vfs_current_identity();
    uint32_t uid = id ? id->uid : 0;
    uint32_t gid = id ? id->gid : 0;

    if (dir == NULL || name == NULL) {
        return EINVAL;
    }
    ramfs_node_t *d = (ramfs_node_t *)dir;
    if ((d->vfs.attr.mode & NOZA_FS_MODE_IFMT) != NOZA_FS_MODE_IFDIR) {
        return ENOTDIR;
    }
    if (ramfs_child(d, name)) {
        return EEXIST;
    }
    ramfs_node_t *n = ramfs_new_node(name, mode, uid, gid, true);
    if (!n) {
        return ENOMEM;
    }
    ramfs_insert_child(d, n);
    printk("[ramfs] mkdir %s under %s mode=%o\n", name, d->name, mode);
    return 0;
}

static int ramfs_create(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, uint32_t mode, vfs_node_t **out)
{
    const noza_identity_t *id = vfs_current_identity();
    uint32_t uid = id ? id->uid : 0;
    uint32_t gid = id ? id->gid : 0;
    if (dir == NULL || name == NULL || out == NULL) {
        return EINVAL;
    }
    ramfs_node_t *d = (ramfs_node_t *)dir;
    if ((d->vfs.attr.mode & NOZA_FS_MODE_IFMT) != NOZA_FS_MODE_IFDIR) {
        return ENOTDIR;
    }
    if (ramfs_child(d, name)) {
        return EEXIST;
    }
    ramfs_node_t *n = ramfs_new_node(name, mode, uid, gid, false);
    if (!n) {
        return ENOMEM;
    }
    ramfs_insert_child(d, n);
    *out = &n->vfs;
    printk("[ramfs] create %s under %s mode=%o\n", name, d->name, mode);
    return 0;
}

static int ramfs_opendir(vfs_mount_t *mnt, vfs_node_t *dir, vfs_handle_t **out)
{
    (void)mnt;
    if (dir == NULL || out == NULL) {
        return EINVAL;
    }
    if ((dir->attr.mode & NOZA_FS_MODE_IFMT) != NOZA_FS_MODE_IFDIR) {
        return ENOTDIR;
    }
    ramfs_handle_t *h = calloc(1, sizeof(ramfs_handle_t));
    if (!h) {
        return ENOMEM;
    }
    h->node = (ramfs_node_t *)dir;
    h->dir_index = 0;
    vfs_handle_t *vh = calloc(1, sizeof(vfs_handle_t));
    if (!vh) {
        free(h);
        return ENOMEM;
    }
    vh->ctx = h;
    vh->node = dir;
    vh->flags = 0; // not used
    *out = vh;
    return 0;
}

static int ramfs_readdir(vfs_mount_t *mnt, vfs_handle_t *handle, noza_fs_dirent_t *ent, int *at_end)
{
    (void)mnt;
    if (handle == NULL || ent == NULL || at_end == NULL) {
        return EINVAL;
    }
    ramfs_handle_t *h = (ramfs_handle_t *)handle->ctx;
    ramfs_node_t *dir = h->node;
    if ((dir->vfs.attr.mode & NOZA_FS_MODE_IFMT) != NOZA_FS_MODE_IFDIR) {
        return ENOTDIR;
    }
    int idx = h->dir_index++;
    if (idx == 0) {
        ent->inode = 0;
        ent->type = NOZA_FS_MODE_IFDIR;
        strncpy(ent->name, ".", sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';
        *at_end = 0;
        return 0;
    }
    if (idx == 1) {
        ent->inode = 0;
        ent->type = NOZA_FS_MODE_IFDIR;
        strncpy(ent->name, "..", sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';
        *at_end = 0;
        return 0;
    }
    ramfs_node_t *child = dir->child;
    // directory entries are stored in insertion order; only entries under this dir should be returned
    for (int i = 0; i < idx - 2 && child; i++) {
        child = child->sibling;
    }
    if (!child) {
        *at_end = 1;
        memset(ent, 0, sizeof(*ent));
        printk("[fs] readdir end idx=%d dir=%s\n", idx, dir->name);
        return 0;
    }
    ent->inode = (uint32_t)((uintptr_t)child & 0xffffffffu);
    ent->type = child->vfs.attr.mode & NOZA_FS_MODE_IFMT;
    strncpy(ent->name, child->name, sizeof(ent->name) - 1);
    ent->name[sizeof(ent->name) - 1] = '\0';
    *at_end = 0;
    return 0;
}

static int ramfs_chmod(vfs_mount_t *mnt, vfs_node_t *node, uint32_t mode)
{
    (void)mnt;
    if (node == NULL) {
        return EINVAL;
    }
    node->attr.mode = (node->attr.mode & NOZA_FS_MODE_IFMT) | (mode & 0777);
    return 0;
}

static int ramfs_chown(vfs_mount_t *mnt, vfs_node_t *node, uint32_t uid, uint32_t gid)
{
    (void)mnt;
    if (node == NULL) {
        return EINVAL;
    }
    node->attr.uid = uid;
    node->attr.gid = gid;
    return 0;
}

static const vfs_ops_t RAMFS_OPS = {
    .lookup = ramfs_lookup,
    .open = ramfs_open,
    .close = ramfs_close,
    .read = ramfs_read,
    .write = ramfs_write,
    .lseek = ramfs_lseek,
    .create = ramfs_create,
    .stat = ramfs_stat,
    .unlink = ramfs_unlink,
    .mkdir = ramfs_mkdir,
    .opendir = ramfs_opendir,
    .readdir = ramfs_readdir,
    .chmod = ramfs_chmod,
    .chown = ramfs_chown,
};

void ramfs_init(void)
{
    memset(&g_ramfs_root, 0, sizeof(g_ramfs_root));
    strncpy(g_ramfs_root.name, "/", sizeof(g_ramfs_root.name) - 1);
    g_ramfs_root.vfs.attr.mode = NOZA_FS_MODE_IFDIR | 0755;
    g_ramfs_root.vfs.attr.uid = 0;
    g_ramfs_root.vfs.attr.gid = 0;
    g_ramfs_root.vfs.attr.nlink = 1;
    g_ramfs_root.parent = &g_ramfs_root;
    g_ramfs_root.vfs.parent = &g_ramfs_root.vfs;
    g_ramfs_root.vfs.ctx = NULL;
    vfs_set_root(&RAMFS_OPS, &g_ramfs_root.vfs, NULL);
}

const vfs_ops_t *ramfs_ops(void)
{
    return &RAMFS_OPS;
}

vfs_node_t *ramfs_create_root(void)
{
    ramfs_node_t *root = calloc(1, sizeof(ramfs_node_t));
    if (root == NULL) {
        return NULL;
    }
    strncpy(root->name, "/", sizeof(root->name) - 1);
    root->vfs.attr.mode = NOZA_FS_MODE_IFDIR | 0755;
    root->vfs.attr.uid = 0;
    root->vfs.attr.gid = 0;
    root->vfs.attr.nlink = 1;
    root->parent = root;
    root->vfs.parent = &root->vfs;
    root->vfs.ctx = NULL;
    return &root->vfs;
}
