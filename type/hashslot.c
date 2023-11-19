#include <string.h>
#include "hashslot.h"

inline static uint8_t hash32to8(uint32_t value)
{
    uint8_t byte1 = (value >> 24) & 0xFF; // High byte
    uint8_t byte2 = (value >> 16) & 0xFF; // Mid-high byte
    uint8_t byte3 = (value >> 8) & 0xFF;  // Mid-low byte
    uint8_t byte4 = value & 0xFF;         // Low byte

    // XOR the chunks together
    return (byte1 ^ byte2 ^ byte3 ^ byte4);
}

void mapping_init(hashslot_t *hashslot)
{
    memset(hashslot, 0, sizeof(hashslot_t));
    noza_spinlock_init(&hashslot->lock);
}

void mapping_insert(hashslot_t *hashslot, uint32_t id, hash_item_t *item, void *value)
{
    int hash = hash32to8(id);
    noza_raw_lock(&hashslot->lock);
    item->id = id;
    item->next = hashslot->items[hash];
    item->value = value;
    hashslot->items[hash] = item;
    noza_spinlock_unlock(&hashslot->lock);
}

void mapping_remove(hashslot_t *hashslot, uint32_t id)
{
    int hash = hash32to8(id);
    noza_raw_lock(&hashslot->lock);
    hash_item_t *prev = NULL;
    hash_item_t *current = hashslot->items[hash];
    while (current) {
        if (current->id == id) {
            if (prev) {
                prev->next = current->next;
            } else {
                hashslot->items[hash] = current->next;
            }
            break;
        }
        prev = current;
        current = current->next;
    }
    noza_spinlock_unlock(&hashslot->lock);
}

void *mapping_get_value(hashslot_t *hashslot, uint32_t id)
{
	int hash = hash32to8(id);
    void *ret_value = NULL;

	noza_raw_lock(&hashslot->lock);
	hash_item_t *item = hashslot->items[hash];
	while (item) {
        if (item->id == id) {
            ret_value = item->value;
            break;
        }
		item = item->next;
	}
	noza_spinlock_unlock(&hashslot->lock);

    return ret_value;
}