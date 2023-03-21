#pragma once
#include "vfs.h"

void init_rootfs();
int rootfs_mount(const char *path, vfs_t *vfs);
int rootfs_unmount(const char *path);

int rootfs_open(const char *path, int flags, int mode);
int rootfs_close(int fd);
int rootfs_read(int fd, void *buf, size_t size);
int rootfs_write(int fd, const void *buf, size_t size);
int rootfs_lseek(int fd, off_t offset, int whence);
int rootfs_stat(const char *path, struct sys_stat *buf);
