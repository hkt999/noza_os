#pragma once
#include <stdint.h>
#include "name_lookup_server.h"

int name_lookup_register(const char *name, uint32_t *service_id);
int name_lookup_resolve(const char *name, uint32_t *service_id, uint32_t *vid);
int name_lookup_resolve_id(uint32_t service_id, uint32_t *vid);
int name_lookup_unregister(uint32_t service_id);
