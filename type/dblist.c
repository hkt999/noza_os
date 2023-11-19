#include "dblist.h"
#include <stddef.h>

dblink_item_t *dblist_insert_tail(dblink_item_t *head, dblink_item_t *item)
{
	if (head == NULL) {
		item->next = item;
		item->prev = item;
		return item;
	}
	item->next = head;
	item->prev = head->prev;
	head->prev->next = item;
	head->prev = item;
	return head;
}

dblink_item_t *dblist_remove_head(dblink_item_t *head)
{
	if (head == NULL) {
		return NULL;
	}
	if (head->next == head) {
		return NULL;
	}
	dblink_item_t *next = head->next;
	head->prev->next = next;
	next->prev = head->prev;
	return next;
}

 dblink_item_t *dblist_remove(dblink_item_t *head, dblink_item_t *obj)
{
    if (head == NULL) {
        return NULL;
    }
    if (head == obj) {
        if (head->next == head && head->prev == head) {
            // only 1 element, and removing head, just clear
            return NULL;
        } else {
            // more than 1 element, and removing head
            dblink_item_t *new_head = head->next;
            new_head->prev = head->prev;
            head->prev->next = new_head;
            return new_head;
        }
    } else {
        // more than 2 elements, and removing object
        obj->prev->next = obj->next;
        obj->next->prev = obj->prev;
        return head;
    }
}