#pragma once
#include <stdint.h>

int name_lookup_register(const char *name, uint32_t value);
int name_lookup_search(const char *name, uint32_t *value);
int name_lookup_unregister(const char *name);