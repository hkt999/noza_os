#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "posix/errno.h"
#include "vfs.h"

#define VFS_R_OK 0x4
#define VFS_W_OK 0x2
#define VFS_X_OK 0x1

static vfs_mount_t *g_mounts;
static vfs_client_t g_clients[VFS_MAX_CLIENTS];
static vfs_mount_t g_root_mount;
static vfs_node_t g_root_node;
static vfs_client_t *g_current_client;

static int null_lookup(vfs_mount_t *mnt, vfs_node_t *dir, const char *name, vfs_node_t **out)
{
    (void)mnt;
    (void)dir;
    (void)name;
    (void)out;
    return ENOENT;
}

static int null_stat(vfs_mount_t *mnt, vfs_node_t *node, noza_fs_attr_t *out)
{
    (void)mnt;
    if (node == NULL || out == NULL) {
        return EINVAL;
    }
    *out = node->attr;
    return 0;
}

static const vfs_ops_t NULL_VFS_OPS = {
    .lookup = null_lookup,
    .stat = null_stat,
};

static int vfs_check_perms(const noza_identity_t *id, const noza_fs_attr_t *attr, uint32_t required)
{
    if (id == NULL || attr == NULL) {
        return EINVAL;
    }
    if (id->uid == 0) {
        return 0; // root bypass
    }

    uint32_t mode = attr->mode;
    uint32_t candidate;
    if (id->uid == attr->uid) {
        candidate = (mode >> 6) & 0x7;
    } else if (id->gid == attr->gid) {
        candidate = (mode >> 3) & 0x7;
    } else {
        candidate = mode & 0x7;
    }

    if ((candidate & required) == required) {
        return 0;
    }
    return EACCES;
}

static bool path_overlap(const char *a, size_t alen, const char *b, size_t blen)
{
    if (alen == 1 && a[0] == '/') {
        return true;
    }
    if (blen == 1 && b[0] == '/') {
        return true;
    }
    if (alen <= blen) {
        return strncmp(a, b, alen) == 0 && (b[alen] == '/' || b[alen] == '\0');
    } else {
        return strncmp(a, b, blen) == 0 && (a[blen] == '/' || a[blen] == '\0');
    }
}

static vfs_mount_t *vfs_match_mount(const char *path, const char **rest)
{
    vfs_mount_t *best = &g_root_mount;
    size_t best_len = g_root_mount.path_len;
    for (vfs_mount_t *m = g_mounts; m; m = m->next) {
        if (m->path_len == 0) {
            continue;
        }
        if (strncmp(path, m->path, m->path_len) == 0) {
            if (path[m->path_len] == '/' || path[m->path_len] == '\0') {
                if (m->path_len > best_len) {
                    best = m;
                    best_len = m->path_len;
                }
            }
        }
    }

    if (rest) {
        *rest = path + best_len;
        if (**rest == '/' && best_len > 1) {
            (*rest)++;
        }
    }
    return best;
}

void vfs_init(void)
{
    memset(g_clients, 0, sizeof(g_clients));
    g_current_client = NULL;

    memset(&g_root_node, 0, sizeof(g_root_node));
    g_root_node.parent = &g_root_node;
    g_root_node.attr.mode = NOZA_FS_MODE_IFDIR | 0755;
    g_root_node.attr.uid = 0;
    g_root_node.attr.gid = 0;
    g_root_node.attr.nlink = 1;

    memset(&g_root_mount, 0, sizeof(g_root_mount));
    strncpy(g_root_mount.path, "/", sizeof(g_root_mount.path) - 1);
    g_root_mount.path_len = 1;
    g_root_mount.ops = &NULL_VFS_OPS;
    g_root_mount.root = &g_root_node;
    g_root_node.mnt = &g_root_mount;
    g_root_mount.next = NULL;
    g_mounts = NULL;
}

int vfs_set_root(const vfs_ops_t *ops, vfs_node_t *root, void *ctx)
{
    if (ops == NULL || root == NULL) {
        return EINVAL;
    }
    g_root_mount.ops = ops;
    g_root_mount.root = root;
    g_root_mount.ctx = ctx;
    g_root_mount.path_len = 1;
    if (root) {
        root->mnt = &g_root_mount;
        root->parent = root->parent ? root->parent : root;
    }
    return 0;
}

int vfs_mount(const char *path, const vfs_ops_t *ops, vfs_node_t *root, void *ctx)
{
    if (path == NULL || ops == NULL || root == NULL) {
        return EINVAL;
    }
    size_t len = strnlen(path, NOZA_FS_MAX_PATH);
    if (len == 0 || len >= NOZA_FS_MAX_PATH) {
        return ENAMETOOLONG;
    }
    if (path[0] != '/') {
        return EINVAL;
    }
    vfs_mount_t *mnt = NULL;
    static vfs_mount_t mount_pool[VFS_MAX_CLIENTS];
    for (size_t i = 0; i < VFS_MAX_CLIENTS; i++) {
        if (mount_pool[i].path_len == 0 && mount_pool[i].root == NULL) {
            mnt = &mount_pool[i];
            break;
        }
    }
    if (mnt == NULL) {
        return ENOMEM;
    }

    // reject overlapping mount points
    for (vfs_mount_t *m = g_mounts; m; m = m->next) {
        if (path_overlap(m->path, m->path_len, mnt->path, mnt->path_len)) {
            return EEXIST;
        }
    }
    if (path_overlap(g_root_mount.path, g_root_mount.path_len, mnt->path, mnt->path_len)) {
        return EEXIST;
    }

    memset(mnt, 0, sizeof(*mnt));
    strncpy(mnt->path, path, sizeof(mnt->path) - 1);
    mnt->path_len = len;
    mnt->ops = ops;
    mnt->root = root;
    mnt->ctx = ctx;
    root->mnt = mnt;
    root->parent = root;

    mnt->next = g_mounts;
    g_mounts = mnt;
    return 0;
}

int vfs_umount(const char *path)
{
    if (path == NULL) {
        return EINVAL;
    }
    size_t len = strnlen(path, NOZA_FS_MAX_PATH);
    if (len == 0 || len >= NOZA_FS_MAX_PATH) {
        return ENAMETOOLONG;
    }
    if (len == 1 && path[0] == '/') {
        return EBUSY; // cannot unmount root
    }

    vfs_mount_t *prev = NULL;
    vfs_mount_t *cur = g_mounts;
    while (cur) {
        if (cur->path_len == len && strncmp(cur->path, path, len) == 0) {
            if (prev) {
                prev->next = cur->next;
            } else {
                g_mounts = cur->next;
            }
            memset(cur, 0, sizeof(*cur));
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }
    return ENOENT;
}

static vfs_client_t *vfs_alloc_client(uint32_t vid, const noza_identity_t *identity)
{
    for (size_t i = 0; i < VFS_MAX_CLIENTS; i++) {
        if (g_clients[i].vid == 0) {
            vfs_client_t *c = &g_clients[i];
            memset(c, 0, sizeof(*c));
            c->vid = vid;
            c->identity = identity ? *identity : (noza_identity_t){0};
            strncpy(c->cwd, "/", sizeof(c->cwd) - 1);
            c->cwd_node = g_root_mount.root ? g_root_mount.root : &g_root_node;
            return c;
        }
    }
    return NULL;
}

vfs_client_t *vfs_client_for_vid(uint32_t vid, const noza_identity_t *identity)
{
    for (size_t i = 0; i < VFS_MAX_CLIENTS; i++) {
        if (g_clients[i].vid == vid) {
            if (identity) {
                g_clients[i].identity = *identity;
            }
            return &g_clients[i];
        }
    }
    return vfs_alloc_client(vid, identity);
}

void vfs_enter_client(vfs_client_t *client)
{
    g_current_client = client;
}

void vfs_leave_client(void)
{
    g_current_client = NULL;
}

const noza_identity_t *vfs_current_identity(void)
{
    if (g_current_client == NULL) {
        return NULL;
    }
    return &g_current_client->identity;
}

static vfs_node_t *vfs_parent_or_self(vfs_node_t *node)
{
    if (node == NULL) {
        return NULL;
    }
    return node->parent ? node->parent : node;
}

static int vfs_walk_components(vfs_node_t *start, const char *path, bool want_parent,
    vfs_node_t **out_node, vfs_node_t **out_parent, char *leaf, size_t leaf_len)
{
    vfs_node_t *cur = start;
    vfs_node_t *parent = vfs_parent_or_self(start);
    const char *cursor = path;
    char component[NOZA_FS_MAX_NAME];

    while (*cursor != '\0') {
        while (*cursor == '/') cursor++;
        if (*cursor == '\0') {
            break;
        }
        const char *next = strchr(cursor, '/');
        size_t comp_len = next ? (size_t)(next - cursor) : strlen(cursor);
        if (comp_len >= sizeof(component)) {
            return ENAMETOOLONG;
        }
        memcpy(component, cursor, comp_len);
        component[comp_len] = '\0';
        cursor = next ? next : cursor + comp_len;

        if (strcmp(component, ".") == 0) {
            continue;
        }
        if (strcmp(component, "..") == 0) {
            cur = vfs_parent_or_self(cur);
            parent = vfs_parent_or_self(cur);
            continue;
        }

        bool last = (*cursor == '\0');
        if (want_parent && last) {
            if (leaf && leaf_len > 0) {
                strncpy(leaf, component, leaf_len - 1);
                leaf[leaf_len - 1] = '\0';
            }
            parent = cur; // parent of the final component is the current node
            break;
        }

        if (cur->mnt == NULL || cur->mnt->ops == NULL || cur->mnt->ops->lookup == NULL) {
            return ENOSYS;
        }
        vfs_node_t *next_node = NULL;
        int rc = cur->mnt->ops->lookup(cur->mnt, cur, component, &next_node);
        if (rc != 0) {
            return rc;
        }
        if (next_node == NULL) {
            return ENOENT;
        }
        next_node->parent = cur;
        cur = next_node;
        parent = vfs_parent_or_self(cur);
    }

    if (out_node) {
        *out_node = cur;
    }
    if (out_parent) {
        *out_parent = parent;
    }
    return 0;
}

static int vfs_resolve(vfs_client_t *client, const char *path, bool want_parent,
    vfs_node_t **out_node, vfs_node_t **out_parent, char *leaf, size_t leaf_len)
{
    if (client == NULL || path == NULL) {
        return EINVAL;
    }

    if (path[0] == '/') {
        const char *rest = NULL;
        vfs_mount_t *mnt = vfs_match_mount(path, &rest);
        vfs_node_t *start = mnt->root;
        return vfs_walk_components(start, (rest && *rest) ? rest : "", want_parent, out_node, out_parent, leaf, leaf_len);
    }

    return vfs_walk_components(client->cwd_node, path, want_parent, out_node, out_parent, leaf, leaf_len);
}

int vfs_get_cwd(vfs_client_t *client, char *buf, size_t buf_len)
{
    if (client == NULL || buf == NULL || buf_len == 0) {
        return EINVAL;
    }
    strncpy(buf, client->cwd, buf_len - 1);
    buf[buf_len - 1] = '\0';
    return 0;
}

int vfs_set_cwd(vfs_client_t *client, const char *path)
{
    if (client == NULL || path == NULL) {
        return EINVAL;
    }
    vfs_node_t *target = NULL;
    int rc = vfs_resolve(client, path, false, &target, NULL, NULL, 0);
    if (rc != 0) {
        return rc;
    }
    noza_fs_attr_t attr = target->attr;
    if (target->mnt && target->mnt->ops && target->mnt->ops->stat) {
        target->mnt->ops->stat(target->mnt, target, &attr);
    }
    if ((attr.mode & NOZA_FS_MODE_IFMT) != NOZA_FS_MODE_IFDIR) {
        return ENOTDIR;
    }
    rc = vfs_check_perms(&client->identity, &attr, VFS_X_OK);
    if (rc != 0) {
        return rc;
    }
    char new_cwd[NOZA_FS_MAX_PATH];
    if (path[0] == '/') {
        strncpy(new_cwd, path, sizeof(new_cwd) - 1);
        new_cwd[sizeof(new_cwd) - 1] = '\0';
    } else {
        int n = snprintf(new_cwd, sizeof(new_cwd), "%s/%s", client->cwd, path);
        if (n < 0 || n >= (int)sizeof(new_cwd)) {
            return ENAMETOOLONG;
        }
    }
    strncpy(client->cwd, new_cwd, sizeof(client->cwd) - 1);
    client->cwd[sizeof(client->cwd) - 1] = '\0';
    client->cwd_node = target;
    return 0;
}

int vfs_umask(vfs_client_t *client, uint32_t new_mask, uint32_t *old_mask)
{
    if (client == NULL) {
        return EINVAL;
    }
    if (old_mask) {
        *old_mask = client->identity.umask;
    }
    if (new_mask != NOZA_FS_UMASK_KEEP) {
        client->identity.umask = new_mask & 0777;
    }
    return 0;
}

static int vfs_alloc_handle(vfs_handle_t **table, size_t max, vfs_handle_t *handle, int *out_fd)
{
    for (size_t i = 0; i < max; i++) {
        if (table[i] == NULL) {
            table[i] = handle;
            if (out_fd) {
                *out_fd = (int)i;
            }
            return 0;
        }
    }
    return EMFILE;
}

static int vfs_check_attr(vfs_node_t *node, noza_fs_attr_t *out)
{
    if (node == NULL || out == NULL) {
        return EINVAL;
    }
    *out = node->attr;
    if (node->mnt && node->mnt->ops && node->mnt->ops->stat) {
        int rc = node->mnt->ops->stat(node->mnt, node, out);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

int vfs_open(vfs_client_t *client, const char *path, uint32_t oflag, uint32_t mode, int *out_fd)
{
    if (client == NULL || path == NULL || out_fd == NULL) {
        return EINVAL;
    }
    vfs_node_t *node = NULL;
    int rc = vfs_resolve(client, path, false, &node, NULL, NULL, 0);
    vfs_node_t *parent = NULL;
    char leaf[NOZA_FS_MAX_NAME];

    if (rc != 0 && (oflag & O_CREAT)) {
        rc = vfs_resolve(client, path, true, NULL, &parent, leaf, sizeof(leaf));
        if (rc != 0) {
            return rc;
        }
        if (parent == NULL || parent->mnt == NULL || parent->mnt->ops == NULL || parent->mnt->ops->create == NULL) {
            return ENOSYS;
        }
        noza_fs_attr_t pattr;
        rc = vfs_check_attr(parent, &pattr);
        if (rc != 0) {
            return rc;
        }
        rc = vfs_check_perms(&client->identity, &pattr, VFS_W_OK | VFS_X_OK);
        if (rc != 0) {
            return rc;
        }
        uint32_t final_mode = (mode == 0 ? 0644 : (mode & 0777));
        final_mode &= ~client->identity.umask;
        rc = parent->mnt->ops->create(parent->mnt, parent, leaf, final_mode, &node);
        if (rc != 0) {
            return rc;
        }
    } else if (rc != 0) {
        return rc;
    }

    noza_fs_attr_t attr;
    rc = vfs_check_attr(node, &attr);
    if (rc != 0) {
        return rc;
    }
    uint32_t acc = oflag & 0x3;
    uint32_t required = (acc == 0) ? VFS_R_OK : (acc == 1 ? VFS_W_OK : (VFS_R_OK | VFS_W_OK));
    rc = vfs_check_perms(&client->identity, &attr, required);
    if (rc != 0) {
        return rc;
    }

    if (node && node->mnt == NULL) {
        return ENOSYS;
    }
    const vfs_ops_t *ops = node ? node->mnt->ops : NULL;
    if (ops == NULL || ops->open == NULL) {
        return ENOSYS;
    }
    uint32_t final_mode = (mode == 0 ? 0644 : (mode & 0777));
    final_mode &= ~client->identity.umask;

    vfs_handle_t *h = NULL;
    rc = ops->open(node ? node->mnt : NULL, node, oflag, final_mode, &h);
    if (rc != 0) {
        return rc;
    }
    if (h == NULL) {
        return EIO;
    }
    h->node = node ? node : h->node;
    h->flags = oflag;
    return vfs_alloc_handle(client->files, VFS_MAX_FD, h, out_fd);
}

int vfs_close(vfs_client_t *client, int fd)
{
    if (client == NULL || fd < 0 || fd >= VFS_MAX_FD) {
        return EINVAL;
    }
    vfs_handle_t *h = client->files[fd];
    if (h == NULL) {
        return EBADF;
    }
    int rc = 0;
    if (h->node && h->node->mnt && h->node->mnt->ops && h->node->mnt->ops->close) {
        rc = h->node->mnt->ops->close(h->node->mnt, h);
    }
    client->files[fd] = NULL;
    if (rc != 0) {
        printf("[fs] close fd=%d rc=%d\n", fd, rc);
    }
    return rc;
}

int vfs_read(vfs_client_t *client, int fd, void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    if (client == NULL || buf == NULL || fd < 0 || fd >= VFS_MAX_FD) {
        return EINVAL;
    }
    vfs_handle_t *h = client->files[fd];
    if (h == NULL) {
        return EBADF;
    }
    if (h->node->mnt == NULL || h->node->mnt->ops == NULL || h->node->mnt->ops->read == NULL) {
        return ENOSYS;
    }
    return h->node->mnt->ops->read(h->node->mnt, h, buf, len, offset, out_len);
}

int vfs_write(vfs_client_t *client, int fd, const void *buf, uint32_t len, uint32_t offset, uint32_t *out_len)
{
    if (client == NULL || buf == NULL || fd < 0 || fd >= VFS_MAX_FD) {
        return EINVAL;
    }
    vfs_handle_t *h = client->files[fd];
    if (h == NULL) {
        return EBADF;
    }
    if (h->node->mnt == NULL || h->node->mnt->ops == NULL || h->node->mnt->ops->write == NULL) {
        return ENOSYS;
    }
    int rc = h->node->mnt->ops->write(h->node->mnt, h, buf, len, offset, out_len);
    if (rc != 0) {
        printf("[fs] write fd=%d rc=%d\n", fd, rc);
    }
    return rc;
}

int vfs_lseek(vfs_client_t *client, int fd, int64_t offset, int32_t whence, int64_t *new_off)
{
    if (client == NULL || fd < 0 || fd >= VFS_MAX_FD) {
        return EINVAL;
    }
    vfs_handle_t *h = client->files[fd];
    if (h == NULL) {
        return EBADF;
    }
    if (h->node->mnt == NULL || h->node->mnt->ops == NULL || h->node->mnt->ops->lseek == NULL) {
        return ENOSYS;
    }
    return h->node->mnt->ops->lseek(h->node->mnt, h, offset, whence, new_off);
}

int vfs_stat_path(vfs_client_t *client, const char *path, noza_fs_attr_t *out)
{
    vfs_node_t *node = NULL;
    int rc = vfs_resolve(client, path, false, &node, NULL, NULL, 0);
    if (rc != 0) {
        return rc;
    }
    return vfs_check_attr(node, out);
}

int vfs_stat_fd(vfs_client_t *client, int fd, noza_fs_attr_t *out)
{
    if (client == NULL || out == NULL || fd < 0 || fd >= VFS_MAX_FD) {
        return EINVAL;
    }
    vfs_handle_t *h = client->files[fd];
    if (h == NULL || h->node == NULL) {
        return EBADF;
    }
    return vfs_check_attr(h->node, out);
}

int vfs_unlink(vfs_client_t *client, const char *path)
{
    vfs_node_t *parent = NULL;
    char leaf[NOZA_FS_MAX_NAME];
    int rc = vfs_resolve(client, path, true, NULL, &parent, leaf, sizeof(leaf));
    if (rc == ENOENT) {
        return 0; // treat missing path as success for unlink cleanup
    }
    if (rc != 0) {
        return rc;
    }
    if (parent == NULL || parent->mnt == NULL || parent->mnt->ops == NULL || parent->mnt->ops->unlink == NULL) {
        return ENOSYS;
    }
    noza_fs_attr_t attr;
    rc = vfs_check_attr(parent, &attr);
    if (rc != 0) {
        return rc;
    }
    rc = vfs_check_perms(&client->identity, &attr, VFS_W_OK | VFS_X_OK);
    if (rc != 0) {
        return rc;
    }
    return parent->mnt->ops->unlink(parent->mnt, parent, leaf);
}

int vfs_mkdir(vfs_client_t *client, const char *path, uint32_t mode)
{
    vfs_node_t *parent = NULL;
    char leaf[NOZA_FS_MAX_NAME];
    int rc = vfs_resolve(client, path, true, NULL, &parent, leaf, sizeof(leaf));
    if (rc != 0) {
        return rc;
    }
    if (parent == NULL || parent->mnt == NULL || parent->mnt->ops == NULL || parent->mnt->ops->mkdir == NULL) {
        return ENOSYS;
    }
    noza_fs_attr_t attr;
    rc = vfs_check_attr(parent, &attr);
    if (rc != 0) {
        return rc;
    }
    rc = vfs_check_perms(&client->identity, &attr, VFS_W_OK | VFS_X_OK);
    if (rc != 0) {
        return rc;
    }
    uint32_t final_mode = (mode & 0777) & ~client->identity.umask;
    return parent->mnt->ops->mkdir(parent->mnt, parent, leaf, final_mode);
}

int vfs_chmod(vfs_client_t *client, const char *path, uint32_t mode)
{
    vfs_node_t *node = NULL;
    int rc = vfs_resolve(client, path, false, &node, NULL, NULL, 0);
    if (rc != 0) {
        return rc;
    }
    if (node->mnt == NULL || node->mnt->ops == NULL || node->mnt->ops->chmod == NULL) {
        return ENOSYS;
    }
    noza_fs_attr_t attr;
    rc = vfs_check_attr(node, &attr);
    if (rc != 0) {
        return rc;
    }
    if (client->identity.uid != 0 && client->identity.uid != attr.uid) {
        return EPERM;
    }
    return node->mnt->ops->chmod(node->mnt, node, mode);
}

int vfs_chown(vfs_client_t *client, const char *path, uint32_t uid, uint32_t gid)
{
    vfs_node_t *node = NULL;
    int rc = vfs_resolve(client, path, false, &node, NULL, NULL, 0);
    if (rc != 0) {
        return rc;
    }
    if (node->mnt == NULL || node->mnt->ops == NULL || node->mnt->ops->chown == NULL) {
        return ENOSYS;
    }
    if (client->identity.uid != 0) {
        return EPERM;
    }
    return node->mnt->ops->chown(node->mnt, node, uid, gid);
}

int vfs_opendir(vfs_client_t *client, const char *path, int *out_fd)
{
    if (client == NULL || path == NULL || out_fd == NULL) {
        return EINVAL;
    }
    vfs_node_t *node = NULL;
    int rc = vfs_resolve(client, path, false, &node, NULL, NULL, 0);
    if (rc != 0) {
        return rc;
    }
    if (node->mnt == NULL || node->mnt->ops == NULL || node->mnt->ops->opendir == NULL) {
        return ENOSYS;
    }
    noza_fs_attr_t attr;
    rc = vfs_check_attr(node, &attr);
    if (rc != 0) {
        return rc;
    }
    if ((attr.mode & NOZA_FS_MODE_IFMT) != NOZA_FS_MODE_IFDIR) {
        return ENOTDIR;
    }
    rc = vfs_check_perms(&client->identity, &attr, VFS_R_OK | VFS_X_OK);
    if (rc != 0) {
        return rc;
    }
    vfs_handle_t *h = NULL;
    rc = node->mnt->ops->opendir(node->mnt, node, &h);
    if (rc != 0) {
        return rc;
    }
    if (h == NULL) {
        return EIO;
    }
    h->node = node;
    h->flags = 0;
    return vfs_alloc_handle(client->dirs, VFS_MAX_DIR, h, out_fd);
}

int vfs_closedir(vfs_client_t *client, int dir_fd)
{
    if (client == NULL || dir_fd < 0 || dir_fd >= VFS_MAX_DIR) {
        return EINVAL;
    }
    vfs_handle_t *h = client->dirs[dir_fd];
    if (h == NULL) {
        return EBADF;
    }
    int rc = 0;
    if (h->node && h->node->mnt && h->node->mnt->ops && h->node->mnt->ops->close) {
        rc = h->node->mnt->ops->close(h->node->mnt, h);
    }
    client->dirs[dir_fd] = NULL;
    return rc;
}

int vfs_readdir(vfs_client_t *client, int dir_fd, noza_fs_dirent_t *ent, int *at_end)
{
    if (client == NULL || ent == NULL || at_end == NULL || dir_fd < 0 || dir_fd >= VFS_MAX_DIR) {
        return EINVAL;
    }
    vfs_handle_t *h = client->dirs[dir_fd];
    if (h == NULL) {
        return EBADF;
    }
    if (h->node == NULL || h->node->mnt == NULL || h->node->mnt->ops == NULL || h->node->mnt->ops->readdir == NULL) {
        return ENOSYS;
    }
    int rc = h->node->mnt->ops->readdir(h->node->mnt, h, ent, at_end);
    if (rc != 0) {
        printf("[fs] readdir fd=%d rc=%d\n", dir_fd, rc);
    }
    return rc;
}
