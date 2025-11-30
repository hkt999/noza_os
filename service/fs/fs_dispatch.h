#pragma once

#include "noza_fs.h"

// Dispatch a parsed FS request into the VFS/backends.
// Returns errno-style codes; resp->code should mirror the returned value.
int fs_dispatch(uint32_t sender_vid, noza_fs_request_t *req, const noza_identity_t *identity, noza_fs_response_t *resp);
