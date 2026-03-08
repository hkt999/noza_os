#include <string.h>
#include "vfs.h"
#include "app_launcher.h"
#include "posix/errno.h"
#include "printk.h"

typedef struct {
    vfs_handle_t handle;
    uint32_t offset;
    app_launcher_msg_t cache;
    uint32_t cache_idx;
} launcher_dir_t;

static vfs_node_t LAUNCHER_ROOT;
static vfs_node_t LAUNCHER_FILE_NODE;
static launcher_dir_t DIR_POOL[VFS_MAX_DIR];

static launcher_dir_t *alloc_dir(void)
{
    for (int i = 0; i < VFS_MAX_DIR; i++) {
        if (DIR_POOL[i].handle.node == NULL) {
            memset(&DIR_POOL[i], 0, sizeof(launcher_dir_t));
            return &DIR_POOL[i];
        }
    }
    return NULL;
}

static void release_dir(launcher_dir_t *ld)
{
    if (ld) {
        memset(ld, 0, sizeof(launcher_dir_t));
    }
}

static int launcher_lookup(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, vfs_node_t **out)
{
    (void)mnt;
    if (dir != &LAUNCHER_ROOT) {
        return ENOENT;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        *out = dir;
        return 0;
    }
    // verify entry exists via list
    uint32_t offset = 0;
    app_launcher_msg_t msg;
    while (1) {
        int rc = app_launcher_list_apps(offset, APP_LAUNCHER_LIST_BATCH, &msg);
        if (rc != 0) {
            return rc;
        }
        for (uint32_t i = 0; i < msg.app_list.count; i++) {
            const char *path = msg.app_list.items[i].path;
            const char *slash = strrchr(path, '/');
            const char *base = slash ? slash + 1 : path;
            if (strncmp(base, name, NOZA_FS_MAX_NAME) == 0) {
                memset(&LAUNCHER_FILE_NODE, 0, sizeof(LAUNCHER_FILE_NODE));
                LAUNCHER_FILE_NODE.mnt = dir->mnt;
                LAUNCHER_FILE_NODE.parent = dir;
                LAUNCHER_FILE_NODE.attr.mode = NOZA_FS_MODE_IFREG | 0755;
                LAUNCHER_FILE_NODE.attr.nlink = 1;
                *out = &LAUNCHER_FILE_NODE;
                return 0;
            }
        }
        if (msg.app_list.count == 0 || offset + msg.app_list.count >= msg.app_list.total) {
            break;
        }
        offset += msg.app_list.count;
    }
    return ENOENT;
}

static int launcher_open(vfs_mount_t *mnt, vfs_node_t *node, uint32_t oflag, uint32_t mode, vfs_handle_t **out)
{
    (void)mnt;
    (void)node;
    (void)oflag;
    (void)mode;
    (void)out;
    return ENOSYS; // read-only listing only
}

static int launcher_close(vfs_mount_t *mnt, vfs_handle_t *handle)
{
    (void)mnt;
    release_dir((launcher_dir_t *)handle);
    return 0;
}

static int launcher_read(vfs_mount_t *mnt, vfs_handle_t *handle, void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    (void)mnt;
    (void)handle;
    (void)buf;
    (void)len;
    (void)offset;
    (void)out_len;
    return ENOSYS;
}

static int launcher_write(vfs_mount_t *mnt, vfs_handle_t *handle, const void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    (void)mnt;
    (void)handle;
    (void)buf;
    (void)len;
    (void)offset;
    (void)out_len;
    return ENOSYS;
}

static int launcher_lseek(vfs_mount_t *mnt, vfs_handle_t *handle, int64_t offset, int32_t whence, int64_t *new_off)
{
    (void)mnt;
    (void)handle;
    (void)offset;
    (void)whence;
    (void)new_off;
    return ENOSYS;
}

static int launcher_create(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, uint32_t mode, vfs_node_t **out)
{
    (void)mnt;
    (void)dir;
    (void)name;
    (void)mode;
    (void)out;
    return ENOSYS;
}

static int launcher_stat(vfs_mount_t *mnt, vfs_node_t *node, noza_fs_attr_t *out)
{
    (void)mnt;
    if (node == NULL || out == NULL) {
        return EINVAL;
    }
    if (node == &LAUNCHER_ROOT) {
        memset(out, 0, sizeof(*out));
        out->mode = NOZA_FS_MODE_IFDIR | 0755;
        out->uid = 0;
        out->gid = 0;
        out->nlink = 1;
    } else {
        *out = node->attr;
    }
    return 0;
}

static int launcher_unlink(vfs_mount_t *mnt, vfs_node_t *dir, const char *name)
{
    (void)mnt;
    (void)dir;
    (void)name;
    return ENOSYS;
}

static int launcher_mkdir(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, uint32_t mode)
{
    (void)mnt;
    (void)dir;
    (void)name;
    (void)mode;
    return ENOSYS;
}

static int launcher_opendir(vfs_mount_t *mnt, vfs_node_t *dir, vfs_handle_t **out)
{
    (void)mnt;
    if (dir != &LAUNCHER_ROOT) {
        return ENOTDIR;
    }
    launcher_dir_t *ld = alloc_dir();
    if (ld == NULL) {
        return ENOMEM;
    }
    ld->handle.node = dir;
    ld->offset = 0;
    ld->cache_idx = 0;
    memset(&ld->cache, 0, sizeof(ld->cache));
    *out = &ld->handle;
    return 0;
}

static int launcher_readdir(vfs_mount_t *mnt, vfs_handle_t *handle, noza_fs_dirent_t *ent, int *at_end)
{
    (void)mnt;
    if (handle == NULL || ent == NULL || at_end == NULL) {
        return EINVAL;
    }
    launcher_dir_t *ld = (launcher_dir_t *)handle;
    if (handle->node != &LAUNCHER_ROOT) {
        return ENOTDIR;
    }

    if (ld->cache_idx >= ld->cache.app_list.count) {
        if (ld->cache.app_list.total != 0 && ld->offset >= ld->cache.app_list.total) {
            *at_end = 1;
            return 0;
        }
        int rc = app_launcher_list_apps(ld->offset, APP_LAUNCHER_LIST_BATCH, &ld->cache);
        if (rc != 0) {
            printk("launcherfs: list apps rc=%d\n", rc);
            return rc;
        }
        ld->cache_idx = 0;
        if (ld->cache.app_list.count == 0) {
            *at_end = 1;
            return 0;
        }
    }

    app_launcher_msg_t *msg = &ld->cache;
    if (ld->cache_idx >= msg->app_list.count) {
        *at_end = 1;
        return 0;
    }
    const char *path = msg->app_list.items[ld->cache_idx].path;
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    memset(ent, 0, sizeof(*ent));
    ent->type = NOZA_FS_MODE_IFREG;
    strncpy(ent->name, base, sizeof(ent->name) - 1);

    ld->cache_idx++;
    ld->offset++;
    *at_end = (ld->offset >= msg->app_list.total);
    return 0;
}

static int launcher_chmod(vfs_mount_t *mnt, vfs_node_t *node, uint32_t mode)
{
    (void)mnt;
    (void)node;
    (void)mode;
    return ENOSYS;
}

static int launcher_chown(vfs_mount_t *mnt, vfs_node_t *node, uint32_t uid, uint32_t gid)
{
    (void)mnt;
    (void)node;
    (void)uid;
    (void)gid;
    return ENOSYS;
}

static const vfs_ops_t LAUNCHER_OPS = {
    .lookup = launcher_lookup,
    .open = launcher_open,
    .close = launcher_close,
    .read = launcher_read,
    .write = launcher_write,
    .lseek = launcher_lseek,
    .create = launcher_create,
    .stat = launcher_stat,
    .unlink = launcher_unlink,
    .mkdir = launcher_mkdir,
    .opendir = launcher_opendir,
    .readdir = launcher_readdir,
    .chmod = launcher_chmod,
    .chown = launcher_chown,
};

int launcherfs_mount(void)
{
    memset(&LAUNCHER_ROOT, 0, sizeof(LAUNCHER_ROOT));
    LAUNCHER_ROOT.attr.mode = NOZA_FS_MODE_IFDIR | 0755;
    LAUNCHER_ROOT.attr.nlink = 1;
    LAUNCHER_ROOT.parent = &LAUNCHER_ROOT;
    memset(&LAUNCHER_FILE_NODE, 0, sizeof(LAUNCHER_FILE_NODE));
    int rc = vfs_mount("/sbin", &LAUNCHER_OPS, &LAUNCHER_ROOT, NULL);
    if (rc != 0) {
        printk("launcherfs: mount /sbin failed rc=%d\n", rc);
    }
    return rc;
}
