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

typedef struct _mutex_item_t {
	dblink_item_t link;
	mutex_pending_t *pending;
	uint32_t lock;
	uint32_t token;
} mutex_item_t;

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

uint32_t mutex_pid;
static int do_mutex_server(void *param, uint32_t pid)
{
	mutex_pid = pid;
	static mutex_item_t mutex_store[MAX_LOCKS];
	static mutex_pending_t pending_store[MAX_PENDING];

	memset(mutex_store, 0, sizeof(mutex_store));
	DBLIST_INIT(mutex_store, MAX_LOCKS);

	memset(pending_store, 0, sizeof(pending_store));
	DBLIST_INIT(pending_store, MAX_PENDING);

	mutex_item_t *mutex_head = &mutex_store[0];
	mutex_pending_t *free_pending_head = &pending_store[0];

	extern uint32_t platform_get_random(void);
	srand(platform_get_random()); // for access token

    noza_msg_t msg;
    for (;;) {
		mutex_item_t *working_mutex = NULL;
        if (noza_recv(&msg) == 0) {
			mutex_msg_t *mutex_msg = (mutex_msg_t *)msg.ptr;
			// sanity check
			if (mutex_msg->cmd != MUTEX_ACQUIRE) {
				if (mutex_msg->mid >= MAX_LOCKS) {
					printf("invalid id: %d\n", mutex_msg->mid);
					mutex_msg->code = MUTEX_INVALID_ID;
					noza_reply(&msg);
					continue;
				}
				working_mutex = &mutex_store[mutex_msg->mid];
				if (working_mutex->token != mutex_msg->token) {
					printf("token mismatch %d != %d\n", mutex_store[mutex_msg->mid].token, mutex_msg->token);
					mutex_msg->code = MUTEX_INVALID_TOKEN;
					noza_reply(&msg);
					continue;
				}
			}

			// process the request
			switch (mutex_msg->cmd) {
				case MUTEX_ACQUIRE:
					if (mutex_head == NULL) {
						printf("no more resource\n");
						mutex_msg->code = MUTEX_NOT_ENOUGH_RESOURCE;
						noza_reply(&msg);
						continue;
					}
					// update the message structure
					mutex_msg->mid = mutex_head->link.index; // assign the link and index
					mutex_msg->token = rand(); // assign new token
					mutex_msg->code = MUTEX_SUCCESS;

					// setup the new head
					mutex_head->token = mutex_msg->token;
					mutex_head->lock = 0;
					if (mutex_head->pending != NULL) {
						// unlikely, handle the exception
						printf("unlikely be here....\n");
					}
					mutex_head = (mutex_item_t *)dblist_remove_head((dblink_item_t *)mutex_head); // move head to next
					noza_reply(&msg);
					break;

				case MUTEX_RELEASE:
					while (working_mutex->pending) {
						mutex_pending_t *pending = working_mutex->pending;
						pending->mutex_msg.code = MUTEX_INVALID_ID; // invalid id
						noza_reply(&pending->pending_msg);
						working_mutex->pending = (mutex_pending_t *)dblist_remove_head(&pending->link);
					}
					working_mutex->token = 0; // clear access token
					working_mutex->lock = 0; // clear lock
					mutex_head = (mutex_item_t *)dblist_insert_tail((dblink_item_t *)mutex_head, &working_mutex->link);
					mutex_msg->code = 0; // success
					noza_reply(&msg);
					break;

				case MUTEX_LOCK:
					if (working_mutex->lock == 0) {
						working_mutex->lock = 1;
						mutex_msg->code = MUTEX_SUCCESS;
						noza_reply(&msg);
					} else {
						mutex_pending_t *w = free_pending_head; // get a pending item from slots
						if (w == NULL) {
							mutex_msg->code = MUTEX_NOT_ENOUGH_RESOURCE;
							noza_reply(&msg);
							continue;
						}
						free_pending_head = (mutex_pending_t *)dblist_remove_head(&free_pending_head->link); // update the pending head

						// setup the pending message
						w->pending_msg = msg;
						w->pending_msg.ptr = &w->mutex_msg;
						w->mutex_msg = *mutex_msg;

						// insert into pending list
						mutex_store[mutex_msg->mid].pending = (mutex_pending_t *)dblist_insert_tail(
							(dblink_item_t *)working_mutex->pending, (dblink_item_t *) w);
					}
					break;

				case MUTEX_TRYLOCK:
					if (working_mutex->lock == 0) {
						working_mutex->lock = 1;
						mutex_msg->code = MUTEX_SUCCESS;
						noza_reply(&msg);
					} else {
						mutex_msg->code = MUTEX_LOCK_FAIL;
						noza_reply(&msg);
					}
					break;

				case MUTEX_UNLOCK:
					if (working_mutex->lock == 0) {
						mutex_msg->code = MUTEX_LOCK_FAIL;
						noza_reply(&msg);
					} else {
						if (working_mutex->pending) { // there is pending request on the queue
							mutex_pending_t *pending = working_mutex->pending; // get pending list
							working_mutex->pending = (mutex_pending_t *)dblist_remove_head((dblink_item_t *)pending); // remove the first item
							working_mutex->lock--; 
							pending->mutex_msg.code = MUTEX_SUCCESS;
							noza_reply(&pending->pending_msg); // reply the first item form pending request
							// put the pending item back to pending list
							free_pending_head = (mutex_pending_t *)dblist_insert_tail((dblink_item_t *)free_pending_head, (dblink_item_t *)pending);
						} else {
							// there is no pending request, clear the lock
							working_mutex->lock = 0; // clear lock
						}
						mutex_msg->code = MUTEX_SUCCESS;
						noza_reply(&msg);
					}
					break;

				default:
					mutex_msg->code = MUTEX_INVALID_OP;
					noza_reply(&msg);
					break;
			}
        }
    }

	return 0;
}

static uint8_t mutex_server_stack[1024];
void __attribute__((constructor(110))) mutex_servier_init(void *param, uint32_t pid)
{
    // TODO: move the external declaraction into a header file
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
	noza_add_service(do_mutex_server, mutex_server_stack, sizeof(mutex_server_stack));
}
