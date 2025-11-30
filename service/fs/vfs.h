#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "noza_fs.h"

#define VFS_MAX_CLIENTS    16
#define VFS_MAX_FD         32
#define VFS_MAX_DIR        16

typedef struct vfs_node vfs_node_t;
typedef struct vfs_mount vfs_mount_t;
typedef struct vfs_handle vfs_handle_t;

typedef struct {
    int (*lookup)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, vfs_node_t **out);
    int (*open)(vfs_mount_t *mnt, vfs_node_t *node, uint32_t oflag, uint32_t mode, vfs_handle_t **out);
    int (*close)(vfs_mount_t *mnt, vfs_handle_t *handle);
    int (*read)(vfs_mount_t *mnt, vfs_handle_t *handle, void *buf, uint32_t len, uint32_t offset, uint32_t *out_len);
    int (*write)(vfs_mount_t *mnt, vfs_handle_t *handle, const void *buf, uint32_t len, uint32_t offset, uint32_t *out_len);
    int (*lseek)(vfs_mount_t *mnt, vfs_handle_t *handle, int64_t offset, int32_t whence, int64_t *new_off);
    int (*create)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, uint32_t mode, vfs_node_t **out);
    int (*stat)(vfs_mount_t *mnt, vfs_node_t *node, noza_fs_attr_t *out);
    int (*unlink)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name);
    int (*mkdir)(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, uint32_t mode);
    int (*opendir)(vfs_mount_t *mnt, vfs_node_t *dir, vfs_handle_t **out);
    int (*readdir)(vfs_mount_t *mnt, vfs_handle_t *handle, noza_fs_dirent_t *ent, int *at_end);
    int (*chmod)(vfs_mount_t *mnt, vfs_node_t *node, uint32_t mode);
    int (*chown)(vfs_mount_t *mnt, vfs_node_t *node, uint32_t uid, uint32_t gid);
} vfs_ops_t;

struct vfs_node {
    vfs_mount_t *mnt;
    vfs_node_t *parent;
    noza_fs_attr_t attr;
    void *ctx;
};

struct vfs_handle {
    vfs_node_t *node;
    uint32_t flags;
    void *ctx;
};

struct vfs_mount {
    char path[NOZA_FS_MAX_PATH];
    size_t path_len;
    const vfs_ops_t *ops;
    vfs_node_t *root;
    void *ctx;
    vfs_mount_t *next;
};

typedef struct {
    uint32_t vid;
    noza_identity_t identity;
    char cwd[NOZA_FS_MAX_PATH];
    vfs_node_t *cwd_node;
    vfs_handle_t *files[VFS_MAX_FD];
    vfs_handle_t *dirs[VFS_MAX_DIR];
} vfs_client_t;

void vfs_init(void);
int vfs_mount(const char *path, const vfs_ops_t *ops, vfs_node_t *root, void *ctx);
int vfs_umount(const char *path);
vfs_client_t *vfs_client_for_vid(uint32_t vid, const noza_identity_t *identity);
void vfs_enter_client(vfs_client_t *client);
void vfs_leave_client(void);
const noza_identity_t *vfs_current_identity(void);
int vfs_set_root(const vfs_ops_t *ops, vfs_node_t *root, void *ctx);
int vfs_get_cwd(vfs_client_t *client, char *buf, size_t buf_len);
int vfs_set_cwd(vfs_client_t *client, const char *path);
int vfs_umask(vfs_client_t *client, uint32_t new_mask, uint32_t *old_mask);

int vfs_open(vfs_client_t *client, const char *path, uint32_t oflag, uint32_t mode, int *out_fd);
int vfs_close(vfs_client_t *client, int fd);
int vfs_read(vfs_client_t *client, int fd, void *buf, uint32_t len, uint32_t offset, uint32_t *out_len);
int vfs_write(vfs_client_t *client, int fd, const void *buf, uint32_t len, uint32_t offset, uint32_t *out_len);
int vfs_lseek(vfs_client_t *client, int fd, int64_t offset, int32_t whence, int64_t *new_off);
int vfs_stat_path(vfs_client_t *client, const char *path, noza_fs_attr_t *out);
int vfs_stat_fd(vfs_client_t *client, int fd, noza_fs_attr_t *out);
int vfs_unlink(vfs_client_t *client, const char *path);
int vfs_mkdir(vfs_client_t *client, const char *path, uint32_t mode);
int vfs_chmod(vfs_client_t *client, const char *path, uint32_t mode);
int vfs_chown(vfs_client_t *client, const char *path, uint32_t uid, uint32_t gid);
int vfs_opendir(vfs_client_t *client, const char *path, int *out_fd);
int vfs_closedir(vfs_client_t *client, int dir_fd);
int vfs_readdir(vfs_client_t *client, int dir_fd, noza_fs_dirent_t *ent, int *at_end);
