#include <string.h>
#include <stdlib.h>
#include "posix/errno.h"
#include "devfs.h"
#include "printk.h"

#define DEVFS_MAX_DEVICES 8

typedef enum {
    DEVFS_NODE_ROOT = 0,
    DEVFS_NODE_DEVICE,
} devfs_node_kind_t;

typedef struct devfs_entry devfs_entry_t;

typedef struct {
    vfs_node_t vfs;
    devfs_node_kind_t kind;
    devfs_entry_t *entry;
} devfs_node_t;

struct devfs_entry {
    devfs_node_t node;
    devfs_device_ops_t ops;
    void *ctx;
    char name[NOZA_FS_MAX_NAME];
    uint8_t in_use;
};

typedef struct {
    devfs_entry_t *entry;
    void *dev_handle;
    uint8_t is_dir;
    uint32_t dir_index;
} devfs_handle_t;

static devfs_node_t g_devfs_root;
static devfs_entry_t g_entries[DEVFS_MAX_DEVICES];
static int g_initialized;

static devfs_entry_t *devfs_find(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (g_entries[i].in_use && strncmp(g_entries[i].name, name, sizeof(g_entries[i].name)) == 0) {
            return &g_entries[i];
        }
    }
    return NULL;
}

static devfs_entry_t *devfs_alloc_entry(void)
{
    for (size_t i = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (!g_entries[i].in_use) {
            memset(&g_entries[i], 0, sizeof(g_entries[i]));
            g_entries[i].in_use = 1;
            return &g_entries[i];
        }
    }
    return NULL;
}

static int devfs_lookup(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, vfs_node_t **out)
{
    (void)mnt;
    if (dir == NULL || name == NULL || out == NULL) {
        return EINVAL;
    }
    devfs_node_t *d = (devfs_node_t *)dir;
    if (d->kind != DEVFS_NODE_ROOT) {
        return ENOTDIR;
    }
    devfs_entry_t *entry = devfs_find(name);
    if (entry == NULL) {
        return ENOENT;
    }
    *out = &entry->node.vfs;
    return 0;
}

static int devfs_open(vfs_mount_t *mnt, vfs_node_t *node, uint32_t oflag, uint32_t mode, vfs_handle_t **out)
{
    (void)mnt;
    if (node == NULL || out == NULL) {
        return EINVAL;
    }
    devfs_node_t *n = (devfs_node_t *)node;
    if (n->kind != DEVFS_NODE_DEVICE || n->entry == NULL) {
        return ENOTSUP;
    }

    devfs_handle_t *h = calloc(1, sizeof(devfs_handle_t));
    if (!h) {
        return ENOMEM;
    }
    h->entry = n->entry;
    h->is_dir = 0;

    if (h->entry->ops.open) {
        int rc = h->entry->ops.open(h->entry->ctx, oflag, mode, &h->dev_handle);
        if (rc != 0) {
            free(h);
            return rc;
        }
    }

    vfs_handle_t *vh = calloc(1, sizeof(vfs_handle_t));
    if (!vh) {
        if (h->entry->ops.close && h->dev_handle) {
            h->entry->ops.close(h->dev_handle);
        }
        free(h);
        return ENOMEM;
    }
    vh->ctx = h;
    vh->node = node;
    vh->flags = oflag;
    *out = vh;
    return 0;
}

static int devfs_close(vfs_mount_t *mnt, vfs_handle_t *handle)
{
    (void)mnt;
    if (handle == NULL) {
        return EINVAL;
    }
    devfs_handle_t *h = (devfs_handle_t *)handle->ctx;
    if (h != NULL && !h->is_dir) {
        if (h->entry && h->entry->ops.close && h->dev_handle) {
            (void)h->entry->ops.close(h->dev_handle);
        }
    }
    free(h);
    free(handle);
    return 0;
}

static int devfs_read(vfs_mount_t *mnt, vfs_handle_t *handle, void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    (void)mnt;
    if (handle == NULL || buf == NULL || out_len == NULL) {
        return EINVAL;
    }
    devfs_handle_t *h = (devfs_handle_t *)handle->ctx;
    if (h == NULL || h->is_dir || h->entry == NULL || h->entry->ops.read == NULL) {
        return ENOTSUP;
    }
    return h->entry->ops.read(h->dev_handle, buf, len, offset, out_len);
}

static int devfs_write(vfs_mount_t *mnt, vfs_handle_t *handle, const void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    (void)mnt;
    if (handle == NULL || buf == NULL || out_len == NULL) {
        return EINVAL;
    }
    devfs_handle_t *h = (devfs_handle_t *)handle->ctx;
    if (h == NULL || h->is_dir || h->entry == NULL || h->entry->ops.write == NULL) {
        return ENOTSUP;
    }
    return h->entry->ops.write(h->dev_handle, buf, len, offset, out_len);
}

static int devfs_lseek(vfs_mount_t *mnt, vfs_handle_t *handle, int64_t offset, int32_t whence, int64_t *new_off)
{
    (void)mnt;
    if (handle == NULL) {
        return EINVAL;
    }
    devfs_handle_t *h = (devfs_handle_t *)handle->ctx;
    if (h == NULL || h->is_dir) {
        return ESPIPE;
    }
    if (h->entry == NULL || h->entry->ops.lseek == NULL) {
        return ESPIPE;
    }
    return h->entry->ops.lseek(h->dev_handle, offset, whence, new_off);
}

static int devfs_stat(vfs_mount_t *mnt, vfs_node_t *node, noza_fs_attr_t *out)
{
    (void)mnt;
    if (node == NULL || out == NULL) {
        return EINVAL;
    }
    *out = node->attr;
    return 0;
}

static int devfs_opendir(vfs_mount_t *mnt, vfs_node_t *dir, vfs_handle_t **out)
{
    (void)mnt;
    if (dir == NULL || out == NULL) {
        return EINVAL;
    }
    devfs_node_t *n = (devfs_node_t *)dir;
    if (n->kind != DEVFS_NODE_ROOT) {
        return ENOTDIR;
    }

    devfs_handle_t *h = calloc(1, sizeof(devfs_handle_t));
    if (!h) {
        return ENOMEM;
    }
    h->is_dir = 1;
    h->dir_index = 0;

    vfs_handle_t *vh = calloc(1, sizeof(vfs_handle_t));
    if (!vh) {
        free(h);
        return ENOMEM;
    }
    vh->ctx = h;
    vh->node = dir;
    vh->flags = 0;
    *out = vh;
    return 0;
}

static int devfs_readdir(vfs_mount_t *mnt, vfs_handle_t *handle, noza_fs_dirent_t *ent, int *at_end)
{
    (void)mnt;
    if (handle == NULL || ent == NULL || at_end == NULL) {
        return EINVAL;
    }
    devfs_handle_t *h = (devfs_handle_t *)handle->ctx;
    if (h == NULL || !h->is_dir) {
        return ENOTDIR;
    }

    // entries: "." , "..", then devices
    if (h->dir_index == 0) {
        strncpy(ent->name, ".", sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';
        ent->type = NOZA_FS_MODE_IFDIR;
        *at_end = 0;
        h->dir_index++;
        return 0;
    }
    if (h->dir_index == 1) {
        strncpy(ent->name, "..", sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';
        ent->type = NOZA_FS_MODE_IFDIR;
        *at_end = 0;
        h->dir_index++;
        return 0;
    }

    uint32_t dev_idx = h->dir_index - 2;
    for (size_t i = 0, seen = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (g_entries[i].in_use) {
            if (seen == dev_idx) {
                strncpy(ent->name, g_entries[i].name, sizeof(ent->name) - 1);
                ent->name[sizeof(ent->name) - 1] = '\0';
                ent->type = NOZA_FS_MODE_IFCHR;
                *at_end = 0;
                h->dir_index++;
                return 0;
            }
            seen++;
        }
    }

    *at_end = 1;
    return 0;
}

static int devfs_unimplemented_dir(vfs_mount_t *mnt, vfs_node_t *dir, const char *name)
{
    (void)mnt;
    (void)dir;
    (void)name;
    return EPERM;
}

static int devfs_chmod(vfs_mount_t *mnt, vfs_node_t *node, uint32_t mode)
{
    (void)mnt;
    (void)node;
    (void)mode;
    return EPERM;
}

static int devfs_chown(vfs_mount_t *mnt, vfs_node_t *node, uint32_t uid, uint32_t gid)
{
    (void)mnt;
    (void)node;
    (void)uid;
    (void)gid;
    return EPERM;
}

static const vfs_ops_t DEVFS_OPS = {
    .lookup = devfs_lookup,
    .open = devfs_open,
    .close = devfs_close,
    .read = devfs_read,
    .write = devfs_write,
    .lseek = devfs_lseek,
    .create = (int (*)(vfs_mount_t *, vfs_node_t *, const char *, uint32_t, vfs_node_t **))devfs_unimplemented_dir,
    .stat = devfs_stat,
    .unlink = (int (*)(vfs_mount_t *, vfs_node_t *, const char *))devfs_unimplemented_dir,
    .mkdir = (int (*)(vfs_mount_t *, vfs_node_t *, const char *, uint32_t))devfs_unimplemented_dir,
    .opendir = devfs_opendir,
    .readdir = devfs_readdir,
    .chmod = devfs_chmod,
    .chown = devfs_chown,
};

int devfs_register_char(const char *name, uint32_t mode, const devfs_device_ops_t *ops, void *ctx)
{
    if (name == NULL || ops == NULL) {
        return EINVAL;
    }
    size_t len = strnlen(name, NOZA_FS_MAX_NAME);
    if (len == 0 || len >= NOZA_FS_MAX_NAME) {
        return ENAMETOOLONG;
    }
    if (devfs_find(name) != NULL) {
        return EEXIST;
    }

    devfs_entry_t *entry = devfs_alloc_entry();
    if (!entry) {
        return ENOMEM;
    }

    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->ops = *ops;
    entry->ctx = ctx;

    entry->node.vfs.parent = &g_devfs_root.vfs;
    entry->node.vfs.mnt = g_devfs_root.vfs.mnt;
    entry->node.vfs.ctx = entry;
    entry->node.vfs.attr.mode = (NOZA_FS_MODE_IFCHR) | (mode & 0777u);
    entry->node.vfs.attr.uid = 0;
    entry->node.vfs.attr.gid = 0;
    entry->node.vfs.attr.nlink = 1;
    entry->node.vfs.attr.size = 0;
    entry->node.vfs.attr.atime_sec = entry->node.vfs.attr.mtime_sec = entry->node.vfs.attr.ctime_sec = 0;
    entry->node.entry = entry;
    entry->node.kind = DEVFS_NODE_DEVICE;
    printk("[devfs] registered /dev/%s\n", entry->name);
    return 0;
}

int devfs_init(void)
{
    if (g_initialized) {
        return 0;
    }

    memset(&g_devfs_root, 0, sizeof(g_devfs_root));
    g_devfs_root.kind = DEVFS_NODE_ROOT;
    g_devfs_root.vfs.parent = &g_devfs_root.vfs;
    g_devfs_root.vfs.mnt = NULL;
    g_devfs_root.vfs.ctx = NULL;
    g_devfs_root.vfs.attr.mode = NOZA_FS_MODE_IFDIR | 0755;
    g_devfs_root.vfs.attr.uid = 0;
    g_devfs_root.vfs.attr.gid = 0;
    g_devfs_root.vfs.attr.nlink = 1;
    g_devfs_root.vfs.attr.size = 0;

    int rc = vfs_mount("/dev", &DEVFS_OPS, &g_devfs_root.vfs, NULL);
    if (rc != 0) {
        printk("[devfs] mount /dev failed rc=%d\n", rc);
        return rc;
    }
    printk("[devfs] mounted at /dev\n");

    // ensure existing entries are attached to the new mount
    for (size_t i = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (g_entries[i].in_use) {
            g_entries[i].node.vfs.mnt = g_devfs_root.vfs.mnt;
            g_entries[i].node.vfs.parent = &g_devfs_root.vfs;
        }
    }

    g_initialized = 1;
    return 0;
}
