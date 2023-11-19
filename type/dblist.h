#pragma once

#include <stdint.h>

typedef struct _dblink_item_t {
	struct _dblink_item_t *prev;
	struct _dblink_item_t *next;
	uint32_t index; // TODO: remove this
} dblink_item_t;

#define DBLIST_INIT(list, count) \
	for (int i = 0; i < count-1; i++) { \
		list[i].link.next = &list[i+1].link; \
	} \
	list[count-1].link.next = &list[0].link; \
	list[0].link.index = 0; \
	for (int i = 1; i < count; i++) { \
		list[i].link.prev = &list[i-1].link; \
		list[i].link.index = i; \
	} \
	list[0].link.prev = &list[count-1].link; 

dblink_item_t *dblist_insert_tail(dblink_item_t *head, dblink_item_t *item);
dblink_item_t *dblist_remove_head(dblink_item_t *head);