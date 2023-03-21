#include "service.h"
#include "rootfs.h"
#include "vfs.h"
#include <service_dirent.h>

// files
int service_open(process_t *p, const char *path, int flags, int mode)
{
    int gfd = rootfs_open(path, flags, mode);
    if (gfd >= 0) {
        for (int i=0; i<MAX_PROCESS_FD; i++) {
            if (p->fdmap[i] == -1) {
                p->fdmap[i] = gfd;
                return i;
            }
        }
        rootfs_close(gfd);
    }
    return -1;
}

int service_close(process_t *p, int fd)
{
    int gfd = p->fdmap[fd];
    return rootfs_close(gfd);
}

size_t service_read(process_t *p, int fd, void *buf, size_t size)
{
    int gfd = p->fdmap[fd];
    return rootfs_read(gfd, buf, size);
}

size_t service_write(process_t *p, int fd, const void *buf, int count)
{
    int gfd = p->fdmap[fd];
    return rootfs_write(gfd, buf, count);
}

off_t service_lseek(process_t *p, int fd, off_t offset, int whence)
{
    int gfd = p->fdmap[fd];
    return rootfs_lseek(gfd, offset, whence);
}

int service_stat(process_t *p, const char *path, struct sys_stat *buf)
{
    return rootfs_stat(path, buf);
}

// directory 
KDIR *service_opendir(process_t *p, const char *filename)
{
    return NULL;
}

int service_closedir(process_t *p, KDIR *dirp)
{
    return 0;
}

struct k_dirent *service_readdir(process_t *p, KDIR *dirp)
{
    return NULL;
}