#pragma once
#include <stdint.h>
#include "spinlock.h"

#define NUM_SLOTS 256

typedef struct hash_item_s {
	void *value;
	uint32_t id;
	struct hash_item_s *next;
} hash_item_t;

typedef struct hashslot_s {
	spinlock_t lock;
	hash_item_t *items[NUM_SLOTS];
} hashslot_t;

void mapping_init(hashslot_t *hashslot);
void mapping_insert(hashslot_t *hashslot, uint32_t id, hash_item_t *item, void *value);
void mapping_remove(hashslot_t *hashslot, uint32_t id);
void *mapping_get_value(hashslot_t *hashslot, uint32_t id);
