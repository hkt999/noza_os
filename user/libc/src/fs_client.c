#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "nozaos.h"
#include "posix/errno.h"
#include "noza_fs.h"
#include "service/name_lookup/name_lookup_client.h"
#include "printk.h"

static uint32_t g_fs_vid;
static uint32_t g_fs_service_id;

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

int noza_open(const char *path, int flags, int mode)
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

int noza_close(int fd)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_CLOSE};
    noza_fs_response_t resp = {0};
    req.close.handle = (uint32_t)fd;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

int noza_read(int fd, void *buf, uint32_t len)
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

int noza_write(int fd, const void *buf, uint32_t len)
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

int64_t noza_lseek(int fd, int64_t offset, int whence)
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

int noza_stat(const char *path, noza_fs_attr_t *st)
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

int noza_fstat(int fd, noza_fs_attr_t *st)
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

int noza_mkdir(const char *path, uint32_t mode)
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

int noza_unlink(const char *path)
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

int noza_chdir(const char *path)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_CHDIR};
    noza_fs_response_t resp = {0};
    if (copy_path(req.path.path, path) != 0) {
        return -1;
    }
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

char *noza_getcwd(char *buf, size_t size)
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

uint32_t noza_umask(uint32_t new_mask)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_UMASK};
    noza_fs_response_t resp = {0};
    req.umask.new_mask = new_mask;
    if (fs_call(&req, &resp) != 0) {
        return (uint32_t)-1;
    }
    return resp.umask.old_mask;
}

int noza_chmod(const char *path, uint32_t mode)
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

int noza_chown(const char *path, uint32_t uid, uint32_t gid)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_CHOWN};
    noza_fs_response_t resp = {0};
    if (copy_path(req.path.path, path) != 0) {
        return -1;
    }
    req.path.uid = uid;
    req.path.gid = gid;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

int noza_opendir(const char *path)
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

int noza_closedir(int dir_fd)
{
    noza_fs_request_t req = {.opcode = NOZA_FS_CLOSEDIR};
    noza_fs_response_t resp = {0};
    req.handle.handle = (uint32_t)dir_fd;
    if (fs_call(&req, &resp) != 0) {
        return -1;
    }
    return 0;
}

int noza_readdir(int dir_fd, noza_fs_dirent_t *ent, int *at_end)
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
