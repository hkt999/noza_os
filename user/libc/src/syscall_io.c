#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include "posix/errno.h"
#include "nozaos.h"
#include "noza_fs.h"
#include "service/name_lookup/name_lookup_client.h"
#include "drivers/uart/uart_io_client.h"
#include "dirent.h"
#include "printk.h"

static uint32_t g_fs_vid;
static uint32_t g_fs_service_id;
static int console_fd = -1;

typedef struct dir_impl {
    int handle;
    struct dirent ent;
    int at_end;
} dir_impl_t;

static int fs_resolve_vid(void)
{
    if (g_fs_vid != 0) {
        return 0;
    }
    int ret = name_lookup_resolve(NOZA_FS_SERVICE_NAME, &g_fs_service_id, &g_fs_vid);
    if (ret != NAME_LOOKUP_OK) {
        g_fs_vid = 0;
        return ESRCH;
    }
    return 0;
}

static int fs_call(noza_fs_request_t *req, noza_fs_response_t *resp)
{
    uint16_t orig_opcode = req ? req->opcode : 0;
    int ret = fs_resolve_vid();
    if (ret != 0) {
        noza_set_errno(ret);
        return -1;
    }
    noza_msg_t msg = {.to_vid = g_fs_vid, .ptr = (void *)req, .size = sizeof(noza_fs_request_t)};
    int ipc = noza_call(&msg);
    if (ipc != 0) {
        g_fs_vid = 0; // force re-resolve
        noza_set_errno(ipc);
        return -1;
    }
    if (resp) {
        memcpy(resp, req, sizeof(noza_fs_response_t)); // kernel/service wrote response into req buffer
    }
    if (resp->code != 0) {
        printk("[fs_client] opcode=%u errno=%d\n", orig_opcode, resp->code);
        noza_set_errno(resp->code);
        return -1;
    }
    return 0;
}

static int copy_path(char dest[NOZA_FS_MAX_PATH], const char *src)
{
    if (src == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    size_t len = strnlen(src, NOZA_FS_MAX_PATH);
    if (len == 0 || len >= NOZA_FS_MAX_PATH) {
        noza_set_errno(ENAMETOOLONG);
        return -1;
    }
    memset(dest, 0, NOZA_FS_MAX_PATH);
    memcpy(dest, src, len);
    dest[len] = '\0';
    return 0;
}

static int fs_open(const char *path, int flags, int mode)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_OPEN};
    noza_fs_response_t resp = {0};
    if (copy_path(req.open.path, path) != 0) {
        return -1;
    }
    req.open.oflag = (uint32_t)flags;
    req.open.mode = (uint32_t)mode;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return (int)resp.open.handle;
}

static int fs_close(int fd)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_CLOSE};
    noza_fs_response_t resp = {0};
    req.close.handle = (uint32_t)fd;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

static int fs_read(int fd, void *buf, uint32_t len)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_READ};
    noza_fs_response_t resp = {0};
    req.rw.handle = (uint32_t)fd;
    req.rw.buf = buf;
    req.rw.length = len;
    req.rw.offset = NOZA_FS_OFFSET_CUR;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return (int)resp.rw.length;
}

static int fs_write(int fd, const void *buf, uint32_t len)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_WRITE};
    noza_fs_response_t resp = {0};
    req.rw.handle = (uint32_t)fd;
    req.rw.buf = (void *)buf;
    req.rw.length = len;
    req.rw.offset = NOZA_FS_OFFSET_CUR;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return (int)resp.rw.length;
}

static int64_t fs_lseek(int fd, int64_t offset, int whence)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_LSEEK};
    noza_fs_response_t resp = {0};
    req.lseek.handle = (uint32_t)fd;
    req.lseek.offset = offset;
    req.lseek.whence = whence;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return resp.lseek.offset;
}

static int fs_stat_path(const char *path, noza_fs_attr_t *st)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_STAT};
    noza_fs_response_t resp = {0};
    if (copy_path(req.path.path, path) != 0) {
        return -1;
    }
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    if (st) {
        *st = resp.stat.attr;
    }
    return 0;
}

static int fs_fstat(int fd, noza_fs_attr_t *st)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_FSTAT};
    noza_fs_response_t resp = {0};
    req.handle.handle = (uint32_t)fd;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    if (st) {
        *st = resp.stat.attr;
    }
    return 0;
}

static int fs_mkdir(const char *path, uint32_t mode)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_MKDIR};
    noza_fs_response_t resp = {0};
    if (copy_path(req.path.path, path) != 0) {
        return -1;
    }
    req.path.mode = mode;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

static int fs_unlink(const char *path)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_UNLINK};
    noza_fs_response_t resp = {0};
    if (copy_path(req.path.path, path) != 0) {
        return -1;
    }
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

static int fs_chdir(const char *path)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_CHDIR};
    noza_fs_response_t resp = {0};
    if (copy_path(req.path.path, path) != 0) {
        return -1;
    }
    if (fs_call(&req, &resp) != 0) {
        printk("[fs_client] chdir send fail path=%s errno=%d\n", req.path.path, noza_get_errno());
        return -1;
    }
    return 0;
}

static char *fs_getcwd(char *buf, size_t size)
{
    if (buf == NULL || size == 0) {
        noza_set_errno(EINVAL);
        return NULL;
    }
    noza_fs_request_t req = {.opcode = NOZA_FS_GETCWD};
    noza_fs_response_t resp = {0};
    req.cwd.buf_size = (uint32_t)size;
    if (fs_call(&req, &resp) != 0) {
        return NULL;
    }
    strncpy(buf, resp.cwd.path, size - 1);
    buf[size - 1] = '\0';
    return buf;
}

static uint32_t fs_umask(uint32_t new_mask)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_UMASK};
    noza_fs_response_t resp = {0};
    req.umask.new_mask = new_mask;
    if (fs_call(&req, &resp) != 0) {
        return (uint32_t)-1;
    }
    return resp.umask.old_mask;
}

static int fs_chmod(const char *path, uint32_t mode)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_CHMOD};
    noza_fs_response_t resp = {0};
    if (copy_path(req.path.path, path) != 0) {
        return -1;
    }
    req.path.mode = mode;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

static int fs_opendir(const char *path)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_OPENDIR};
    noza_fs_response_t resp = {0};
    if (copy_path(req.path.path, path) != 0) {
        return -1;
    }
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return (int)resp.open.handle;
}

static int fs_closedir(int dir_fd)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_CLOSEDIR};
    noza_fs_response_t resp = {0};
    req.handle.handle = (uint32_t)dir_fd;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

static int fs_readdir(int dir_fd, noza_fs_dirent_t *ent, int *at_end)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_READDIR};
    noza_fs_response_t resp = {0};
    req.handle.handle = (uint32_t)dir_fd;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    if (ent) {
        *ent = resp.dir.entry;
    }
    if (at_end) {
        *at_end = resp.dir.at_end;
    }
    return 0;
}

static int ensure_console_fd(void) {
    if (console_fd >= 0) {
        return console_fd;
    }
    int fd = fs_open("/dev/ttyS0", O_RDWR, 0666);
    if (fd >= 0) {
        console_fd = fd;
        return console_fd;
    }
    return -1;
}

static int console_write_wrapper(const void *buf, size_t count) {
    int fd = ensure_console_fd();
    if (fd >= 0) {
        int w = fs_write(fd, buf, (uint32_t)count);
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
        int r = fs_read(fd, buf, (uint32_t)count);
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
    return fs_write(fd, buf, (uint32_t)count);
}

int _read(int fd, void *buf, size_t count) {
    if (buf == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    if (fd == STDIN_FILENO) {
        return console_read_wrapper(buf, count);
    }
    return fs_read(fd, buf, (uint32_t)count);
}

off_t _lseek(int fd, off_t offset, int whence) {
    int64_t r = fs_lseek(fd, (int64_t)offset, whence);
    return (off_t)r;
}

int _close(int fd) {
    int rc = fs_close(fd);
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
    if (fs_fstat(fd, &attr) == 0) {
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
    int fd = fs_open(path, (uint32_t)flags, (uint32_t)mode);
    if (fd < 0) {
        return -1;
    }
    return fd;
}

int _unlink(const char *path) {
    return fs_unlink(path);
}

int _stat(const char *path, struct stat *st) {
    if (path == NULL || st == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    noza_fs_attr_t attr;
    if (fs_stat_path(path, &attr) != 0) {
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
    return fs_mkdir(path, (uint32_t)mode);
}

int _chdir(const char *path) {
    if (path == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    return fs_chdir(path);
}

char *_getcwd(char *buf, size_t size) {
    return fs_getcwd(buf, size);
}

int _chmod(const char *path, mode_t mode) {
    if (path == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    return fs_chmod(path, (uint32_t)mode);
}

int _fchmod(int fd, mode_t mode) {
    (void)fd;
    (void)mode;
    noza_set_errno(ENOSYS);
    return -1;
}

mode_t _umask(mode_t cmask) {
    return (mode_t)fs_umask((uint32_t)cmask);
}

int _ftruncate(int fd, off_t length) {
    (void)fd;
    (void)length;
    noza_set_errno(ENOSYS);
    return -1;
}

DIR *opendir(const char *path) {
    if (path == NULL) {
        noza_set_errno(EINVAL);
        return NULL;
    }
    int handle = fs_opendir(path);
    if (handle < 0) {
        return NULL;
    }
    dir_impl_t *impl = (dir_impl_t *)malloc(sizeof(dir_impl_t));
    if (impl == NULL) {
        fs_closedir(handle);
        noza_set_errno(ENOMEM);
        return NULL;
    }
    memset(impl, 0, sizeof(*impl));
    impl->handle = handle;
    return (DIR *)impl;
}

struct dirent *readdir(DIR *dirp) {
    if (dirp == NULL) {
        noza_set_errno(EINVAL);
        return NULL;
    }
    dir_impl_t *impl = (dir_impl_t *)dirp;
    if (impl->at_end) {
        return NULL;
    }
    noza_fs_dirent_t ent;
    int at_end = 0;
    if (fs_readdir(impl->handle, &ent, &at_end) != 0) {
        return NULL;
    }
    impl->at_end = at_end;
    memset(&impl->ent, 0, sizeof(impl->ent));
    strncpy(impl->ent.d_name, ent.name, sizeof(impl->ent.d_name) - 1);
    return &impl->ent;
}

int closedir(DIR *dirp) {
    if (dirp == NULL) {
        noza_set_errno(EINVAL);
        return -1;
    }
    dir_impl_t *impl = (dir_impl_t *)dirp;
    fs_closedir(impl->handle);
    free(impl);
    return 0;
}

// libc-facing aliases
int mkdir(const char *path, mode_t mode) { return _mkdir(path, mode); }
int chdir(const char *path) { return _chdir(path); }
char *getcwd(char *buf, size_t size) { return _getcwd(buf, size); }
int chmod(const char *path, mode_t mode) { return _chmod(path, mode); }
mode_t umask(mode_t cmask) { return _umask(cmask); }
