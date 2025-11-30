#pragma once

#include "vfs.h"

void ramfs_init(void);
const vfs_ops_t *ramfs_ops(void);
vfs_node_t *ramfs_create_root(void);
