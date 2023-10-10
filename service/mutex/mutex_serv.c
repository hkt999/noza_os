#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mutex_serv.h"
#include "nozaos.h"

typedef struct _dblink_item_t {
	struct _dblink_item_t *prev;
	struct _dblink_item_t *next;
	uint32_t index;
} dblink_item_t;

typedef struct _mutex_pending_t {
	dblink_item_t link;
	noza_msg_t pending_msg;
	mutex_msg_t mutex_msg;
} mutex_pending_t;

typedef struct _mutex_store_t {
	dblink_item_t link;
	mutex_pending_t *pending;
	uint32_t lock;
	uint32_t token;
} mutex_store_t;

#define DBLIST_INIT(list, count) \
	for (int i = 0; i < count-1; i++) { \
		list[i].link.next = &list[i+1].link; \
	} \
	list[count-1].link.next = &list[0].link; \
	list[0].link.index = 0; \
	for (int i = 1; i < count; i++) { \
		list[i].link.prev = &list[i-1].link; \
		list[0].link.index = i; \
	} \
	list[0].link.prev = &list[count-1].link; 

static dblink_item_t *dblist_insert_tail(dblink_item_t *head, dblink_item_t *item)
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

static dblink_item_t *dblist_remove_head(dblink_item_t *head)
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

static void do_mutex_server(void *param, uint32_t pid)
{
	printf("do_mutex_server pid=%d\n", pid);
	static mutex_store_t mutex_store[MAX_LOCKS];
	memset(mutex_store, 0, sizeof(mutex_store));
	DBLIST_INIT(mutex_store, MAX_LOCKS);
	mutex_store_t *mutex_head = &mutex_store[0];

	static mutex_pending_t pending_store[MAX_PENDING];
	memset(pending_store, 0, sizeof(pending_store));
	DBLIST_INIT(pending_store, MAX_PENDING);
	mutex_pending_t *pending_head = &pending_store[0];

	extern uint32_t platform_get_random(void);
	srand(platform_get_random()); // token

    noza_msg_t msg;
    for (;;) {
        if (noza_recv(&msg) == 0) {
			printf("mutex server got msg: %s\n", (char *)msg.ptr);
			mutex_msg_t *mutex_msg = (mutex_msg_t *)msg.ptr;
			// sanity check
			if (mutex_msg->mid >= MAX_LOCKS) {
				mutex_msg->code = MUTEX_INVALID_ID;
				noza_reply(&msg);
				continue;
			}
			if (mutex_msg->cmd != MUTEX_ACQUIRE) {
				if (mutex_store[mutex_msg->mid].token != mutex_msg->token) {
					mutex_msg->code = MUTEX_INVALID_TOKEN;
					noza_reply(&msg);
					continue;
				}
			}
			switch (mutex_msg->cmd) {
				case MUTEX_ACQUIRE:
					if (mutex_head == NULL) {
						mutex_msg->code = MUTEX_NOT_ENOUGH_RESOURCE;
						noza_reply(&msg);
						continue;
					}
					mutex_msg->mid = mutex_head->link.index;
					mutex_msg->token = rand();
					mutex_msg->code = MUTEX_SUCCESS;
					mutex_head->token = mutex_msg->token;
					mutex_head->lock = 0;
					assert(mutex_head->pending == NULL);
					mutex_head = (mutex_store_t *)dblist_remove_head(&mutex_head->link); // move head to next
					noza_reply(&msg);
					break;

				case MUTEX_RELEASE:
					while (mutex_store[mutex_msg->mid].pending) {
						mutex_pending_t *pending = mutex_store[mutex_msg->mid].pending;
						pending->mutex_msg.code = MUTEX_SUCCESS; // auto unlock
						noza_reply(&pending->pending_msg);
						mutex_store[mutex_msg->mid].pending = (mutex_pending_t *)dblist_remove_head(&pending->link);
					}
					mutex_store[mutex_msg->mid].lock = 0; // clear lock
					mutex_head = (mutex_store_t *)dblist_insert_tail((dblink_item_t *)mutex_head, &mutex_store[mutex_msg->mid].link);
					mutex_msg->code = 0; // success
					noza_reply(&msg);
					break;

				case MUTEX_LOCK:
					if (mutex_store[mutex_msg->mid].lock == 0) {
						mutex_store[mutex_msg->mid].lock = 1;
						mutex_msg->code = MUTEX_SUCCESS;
						noza_reply(&msg);
					} else {
						mutex_pending_t *w = pending_head;
						if (w == NULL) {
							mutex_msg->code = MUTEX_NOT_ENOUGH_RESOURCE;
							noza_reply(&msg);
							continue;
						}
						pending_head = (mutex_pending_t *)dblist_remove_head(&pending_head->link);
						w->pending_msg = msg;
						w->pending_msg.ptr = &w->mutex_msg;
						w->mutex_msg = *mutex_msg;
						mutex_store[mutex_msg->mid].pending = (mutex_pending_t *)dblist_insert_tail(
							(dblink_item_t *)mutex_store[mutex_msg->mid].pending, &w->link);
					}
					break;

				case MUTEX_TRYLOCK:
					if (mutex_store[mutex_msg->mid].lock == 0) {
						mutex_store[mutex_msg->mid].lock = 1;
						mutex_msg->code = MUTEX_SUCCESS;
						noza_reply(&msg);
					} else {
						mutex_msg->code = MUTEX_LOCK_FAIL;
						noza_reply(&msg);
					}
					break;

				case MUTEX_UNLOCK:
					if (mutex_store[mutex_msg->mid].lock == 0) {
						mutex_msg->code = MUTEX_LOCK_FAIL;
						noza_reply(&msg);
					} else {
						if (mutex_store[mutex_msg->mid].pending) {
							mutex_pending_t *pending = mutex_store[mutex_msg->mid].pending;
							mutex_store[mutex_msg->mid].pending = (mutex_pending_t *)dblist_remove_head(&pending->link);
							mutex_store[mutex_msg->mid].lock--; 
							pending->mutex_msg.code = MUTEX_SUCCESS;
							noza_reply(&pending->pending_msg);
						} else {
							// there is no pending request, clear the lock
							mutex_store[mutex_msg->mid].lock = 0; // clear lock
							mutex_msg->code = MUTEX_SUCCESS;
							noza_reply(&msg);
						}
					}
					break;

				default:
					mutex_msg->code = MUTEX_INVALID_OP;
					noza_reply(&msg);
					break;
			}
        }
    }
}

uint32_t mutex_pid;
void __attribute__((constructor(110))) mutex_servier_init(void *param, uint32_t pid)
{
	mutex_pid = pid;
    // TODO: move the external declaraction into a header file
    extern void noza_add_service(void (*entry)(void *param, uint32_t pid));
	noza_add_service(do_mutex_server);
}
