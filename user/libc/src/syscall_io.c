#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "posix/errno.h"
#include "nozaos.h"
#include "noza_fs.h"
#include "drivers/uart/uart_io_client.h"

static int console_fd = -1;

static int ensure_console_fd(void) {
    if (console_fd >= 0) {
        return console_fd;
    }
    int fd = noza_open("/dev/ttyS0", O_RDWR, 0666);
    if (fd >= 0) {
        console_fd = fd;
        return console_fd;
    }
    return -1;
}

static int console_write_wrapper(const void *buf, size_t count) {
    int fd = ensure_console_fd();
    if (fd >= 0) {
        int w = noza_write(fd, buf, (uint32_t)count);
        if (w >= 0) {
            return w;
        }
    }
    int ret = console_write((const char *)buf, (uint32_t)count);
    if (ret != 0) {
        noza_set_errno(ret);
        return -1;
    }
    return (int)count;
}

static int console_read_wrapper(void *buf, size_t count) {
    int fd = ensure_console_fd();
    if (fd >= 0) {
        int r = noza_read(fd, buf, (uint32_t)count);
        if (r >= 0) {
            return r;
        }
    }
    uint32_t out_len = 0;
    int ret = console_readline((char *)buf, (uint32_t)count, &out_len);
    if (ret != 0) {
        noza_set_errno(ret);
        return -1;
    }
    return (int)out_len;
}

static void fs_attr_to_stat(const noza_fs_attr_t *attr, struct stat *st)
{
    if (attr == NULL || st == NULL) {
        return;
    }
    mode_t type = S_IFREG;
    switch (attr->mode & NOZA_FS_MODE_IFMT) {
        case NOZA_FS_MODE_IFDIR:
            type = S_IFDIR;
            break;
        case NOZA_FS_MODE_IFCHR:
            type = S_IFCHR;
            break;
        default:
            type = S_IFREG;
            break;
    }
    st->st_mode = type | (attr->mode & 0777u);
    st->st_nlink = attr->nlink;
    st->st_uid = attr->uid;
    st->st_gid = attr->gid;
    st->st_size = (off_t)attr->size;
    st->st_atime = (time_t)attr->atime_sec;
    st->st_mtime = (time_t)attr->mtime_sec;
    st->st_ctime = (time_t)attr->ctime_sec;
}

int _write(int fd, const void *buf, size_t count) {
    if (buf == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return console_write_wrapper(buf, count);
    }
    return noza_write(fd, buf, (uint32_t)count);
}

int _read(int fd, void *buf, size_t count) {
    if (buf == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    if (fd == STDIN_FILENO) {
        return console_read_wrapper(buf, count);
    }
    return noza_read(fd, buf, (uint32_t)count);
}

off_t _lseek(int fd, off_t offset, int whence) {
    int64_t r = noza_lseek(fd, (int64_t)offset, whence);
    return (off_t)r;
}

int _close(int fd) {
    int rc = noza_close(fd);
    if (fd == console_fd) {
        console_fd = -1;
    }
    return rc;
}

int _fstat(int fd, struct stat *st) {
    if (st == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    memset(st, 0, sizeof(*st));
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO || fd == console_fd) {
        st->st_mode = S_IFCHR;
        return 0;
    }
    noza_fs_attr_t attr;
    if (noza_fstat(fd, &attr) == 0) {
        fs_attr_to_stat(&attr, st);
        return 0;
    }
    noza_set_errno(ENOSYS);
    return -1;
}

int _isatty(int fd) {
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO || fd == console_fd) {
        return 1;
    }
    noza_set_errno(ENOTTY);
    return 0;
}

int _open(const char *path, int flags, int mode) {
    int fd = noza_open(path, (uint32_t)flags, (uint32_t)mode);
    if (fd < 0) {
        return -1;
    }
    return fd;
}

int _unlink(const char *path) {
    return noza_unlink(path);
}

int _stat(const char *path, struct stat *st) {
    if (path == NULL || st == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    noza_fs_attr_t attr;
    if (noza_stat(path, &attr) != 0) {
        return -1;
    }
    fs_attr_to_stat(&attr, st);
    return 0;
}

int _mkdir(const char *path, mode_t mode) {
    if (path == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    return noza_mkdir(path, (uint32_t)mode);
}

int _chdir(const char *path) {
    if (path == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    return noza_chdir(path);
}

char *_getcwd(char *buf, size_t size) {
    return noza_getcwd(buf, size);
}
