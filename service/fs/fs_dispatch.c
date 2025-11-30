#include <string.h>
#include "posix/errno.h"
#include "fs_dispatch.h"
#include "vfs.h"
#include "ramfs.h"

static int map_read_write(vfs_client_t *client, noza_fs_request_t *req, noza_fs_response_t *resp)
{
    uint32_t transferred = 0;
    if (req->opcode == NOZA_FS_READ) {
        resp->code = vfs_read(client, (int)req->rw.handle, req->rw.buf, req->rw.length, req->rw.offset, &transferred);
    } else {
        resp->code = vfs_write(client, (int)req->rw.handle, req->rw.buf, req->rw.length, req->rw.offset, &transferred);
    }
    resp->rw.length = transferred;
    return resp->code;
}

int fs_dispatch(uint32_t sender_vid, noza_fs_request_t *req, const noza_identity_t *identity, noza_fs_response_t *resp)
{
    if (req == NULL || resp == NULL) {
        return EINVAL;
    }

    vfs_client_t *client = vfs_client_for_vid(sender_vid, identity);
    if (client == NULL) {
        resp->code = ENOMEM;
        return resp->code;
    }

    resp->code = 0;
    vfs_enter_client(client);
    switch (req->opcode) {
    case NOZA_FS_OPEN:
        resp->code = vfs_open(client, req->open.path, req->open.oflag, req->open.mode, (int *)&resp->open.handle);
        break;
    case NOZA_FS_CLOSE:
        resp->code = vfs_close(client, (int)req->close.handle);
        break;
    case NOZA_FS_READ:
    case NOZA_FS_WRITE:
        map_read_write(client, req, resp);
        break;
    case NOZA_FS_LSEEK: {
        int64_t new_off = 0;
        resp->code = vfs_lseek(client, (int)req->lseek.handle, req->lseek.offset, req->lseek.whence, &new_off);
        resp->lseek.offset = new_off;
        break;
    }
    case NOZA_FS_STAT:
        resp->code = vfs_stat_path(client, req->path.path, &resp->stat.attr);
        break;
    case NOZA_FS_FSTAT:
        resp->code = vfs_stat_fd(client, (int)req->handle.handle, &resp->stat.attr);
        break;
    case NOZA_FS_OPENDIR:
        resp->code = vfs_opendir(client, req->path.path, (int *)&resp->open.handle);
        break;
    case NOZA_FS_READDIR:
        resp->code = vfs_readdir(client, (int)req->handle.handle, &resp->dir.entry, (int *)&resp->dir.at_end);
        break;
    case NOZA_FS_CLOSEDIR:
        resp->code = vfs_closedir(client, (int)req->handle.handle);
        break;
    case NOZA_FS_MKDIR:
        resp->code = vfs_mkdir(client, req->path.path, req->path.mode);
        break;
    case NOZA_FS_UNLINK:
        resp->code = vfs_unlink(client, req->path.path);
        break;
    case NOZA_FS_CHDIR:
        resp->code = vfs_set_cwd(client, req->path.path);
        break;
    case NOZA_FS_GETCWD:
        resp->code = vfs_get_cwd(client, resp->cwd.path, sizeof(resp->cwd.path));
        break;
    case NOZA_FS_UMASK:
        resp->code = vfs_umask(client, req->umask.new_mask, &resp->umask.old_mask);
        break;
    case NOZA_FS_CHMOD:
        resp->code = vfs_chmod(client, req->path.path, req->path.mode);
        break;
    case NOZA_FS_CHOWN:
        resp->code = vfs_chown(client, req->path.path, req->path.uid, req->path.gid);
        break;
    case NOZA_FS_MOUNT: {
        const vfs_ops_t *ops = NULL;
        vfs_node_t *root = NULL;
        switch (req->mount.fs_type) {
        case 0: // ramfs
            ops = ramfs_ops();
            root = ramfs_create_root();
            break;
        default:
            resp->code = ENODEV;
            break;
        }
        if (resp->code == 0) {
            if (ops == NULL || root == NULL) {
                resp->code = ENODEV;
            } else {
                resp->code = vfs_mount(req->mount.target, ops, root, NULL);
            }
        }
        break;
    }
    case NOZA_FS_UMOUNT:
        resp->code = vfs_umount(req->path.path);
        break;
    default:
        resp->code = EINVAL;
        break;
    }
    vfs_leave_client();

    return resp->code;
}
