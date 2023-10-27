#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "sync_serv.h"
#include "nozaos.h"

typedef struct _dblink_item_t {
	struct _dblink_item_t *prev;
	struct _dblink_item_t *next;
	uint32_t index;
} dblink_item_t;

typedef struct _mutex_pending_t {
	dblink_item_t link;
	noza_msg_t noza_msg; // pending noza message
} pending_node_t;

typedef struct _mutex_item_t {
	dblink_item_t link;
	pending_node_t *pending;
	uint32_t lock;
	uint32_t token;
} mutex_item_t;

typedef struct _cond_item_t {
	dblink_item_t link;
	pending_node_t *pending;
	uint32_t signaled;
	uint32_t ctoken;
} cond_item_t;

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

typedef struct {
	mutex_item_t	mutex_store[MAX_LOCKS];
	mutex_item_t	*mutex_head;

	cond_item_t		cond_store[MAX_CONDS];
	cond_item_t		*cond_head;

	pending_node_t	*free_pending_head;
	pending_node_t	pending_store[MAX_PENDING];
} sync_info_t;

static inline void mutex_insert_pending_tail(mutex_item_t *mutex, pending_node_t *pm)
{
	mutex->pending = (pending_node_t *)dblist_insert_tail(&mutex->pending->link, &pm->link);
}

static inline pending_node_t *get_free_pending_slot(sync_info_t *si)
{
	pending_node_t *head = si->free_pending_head;
	si->free_pending_head = (pending_node_t *)dblist_remove_head(&head->link);
	return head;
}

static inline void insert_free_padding_tail(sync_info_t *si, pending_node_t *pm)
{
	si->free_pending_head = (pending_node_t *)dblist_insert_tail(&si->free_pending_head->link, &pm->link);
}

static inline pending_node_t *mutex_get_pending_head(mutex_item_t *working_mutex)
{
	pending_node_t *head = working_mutex->pending; // get pending list
	working_mutex->pending = (pending_node_t *)dblist_remove_head(&head->link); // remove the first item
	return head;
}

static void mutex_server_acquire(noza_msg_t *msg, sync_info_t *si)
{
	mutex_msg_t *mutex_msg = (mutex_msg_t *)msg->ptr;
	if (si->mutex_head == NULL) {
		printf("mutex: no more resource\n");
		mutex_msg->code = MUTEX_NOT_ENOUGH_RESOURCE;
		noza_reply(msg);
		return;
	}
	// update the message structure
	mutex_msg->mid = si->mutex_head->link.index; // assign the link and index
	mutex_msg->token = rand(); // assign new token
	mutex_msg->code = MUTEX_SUCCESS;

	// setup the new head
	si->mutex_head->token = mutex_msg->token;
	si->mutex_head->lock = 0;
	if (si->mutex_head->pending != NULL) {
		// unlikely, handle the exception
		printf("mutex: unlikely be here....\n");
	}
	si->mutex_head = (mutex_item_t *)dblist_remove_head((dblink_item_t *)si->mutex_head); // move head to next
	noza_reply(msg);
}

static void mutex_server_release(mutex_item_t *working_mutex, noza_msg_t *msg, sync_info_t *si)
{
	mutex_msg_t *mutex_msg = (mutex_msg_t *)msg->ptr;
	while (working_mutex->pending) {
		pending_node_t *pending = working_mutex->pending;
		mutex_msg_t *mg = (mutex_msg_t *)pending->noza_msg.ptr;
		mg->code = MUTEX_INVALID_ID; // invalid id
		noza_reply(&pending->noza_msg);
		working_mutex->pending = (pending_node_t *)dblist_remove_head(&pending->link);
	}
	working_mutex->token = 0; // clear access token
	working_mutex->lock = 0; // clear lock
	si->mutex_head = (mutex_item_t *)dblist_insert_tail((dblink_item_t *)si->mutex_head, &working_mutex->link);
	mutex_msg->code = MUTEX_SUCCESS; // success
	noza_reply(msg);
}

static void mutex_server_lock(mutex_item_t *working_mutex, noza_msg_t *msg, sync_info_t *si)
{
	mutex_msg_t *mutex_msg = (mutex_msg_t *)msg->ptr;
	if (working_mutex->lock == 0) {
		working_mutex->lock = 1; // lock
		mutex_msg->code = MUTEX_SUCCESS;
		noza_reply(msg); // the lock is free, lock it and return immediately
	} else {
		pending_node_t *pm = get_free_pending_slot(si);
		if (pm == NULL) {
			mutex_msg->code = MUTEX_NOT_ENOUGH_RESOURCE;
			printf("mutex: no more free pending slot, just lock and reply\n");
			noza_reply(msg);
			return;
		}
		pm->noza_msg = *msg; // copy message
		mutex_insert_pending_tail(working_mutex, pm);
	}
}

static void mutex_server_trylock(mutex_item_t *working_mutex, noza_msg_t *msg, sync_info_t *si)
{
	mutex_msg_t *mutex_msg = (mutex_msg_t *)msg->ptr;
	if (working_mutex->lock == 0) {
		working_mutex->lock = 1;
		mutex_msg->code = MUTEX_SUCCESS;
	} else {
		mutex_msg->code = MUTEX_LOCK_FAIL;
	}
	noza_reply(msg);
}

static void mutex_server_unlock(mutex_item_t *working_mutex, noza_msg_t *msg, sync_info_t *si)
{
	mutex_msg_t *mutex_msg = NULL;
	if (msg) mutex_msg = (mutex_msg_t *)msg->ptr;

	pending_node_t *pending = mutex_get_pending_head(working_mutex);
	if (pending) { // there is pending request on the queue, release one
		mutex_msg_t *m = (mutex_msg_t *)pending->noza_msg.ptr;
		m->code = MUTEX_SUCCESS;
		noza_reply(&pending->noza_msg); // reply the first item form pending request
		insert_free_padding_tail(si, pending);
		if (mutex_msg) {
			mutex_msg->code = MUTEX_SUCCESS; // reply success
			noza_reply(msg);
		}
	} else {
		// there is no pending request, unlock the request
		working_mutex->lock = 0;
		if (mutex_msg) {
			mutex_msg->code = MUTEX_SUCCESS;
			noza_reply(msg);
		}
	}
}

static void process_mutex(noza_msg_t *msg, sync_info_t *si)
{
	mutex_item_t *working_mutex = NULL;
	mutex_msg_t *mutex_msg = (mutex_msg_t *)msg->ptr;
	// sanity check
	if (mutex_msg->cmd != MUTEX_ACQUIRE) {
		if (mutex_msg->mid >= MAX_LOCKS) {
			printf("mutex invalid id: %d\n", mutex_msg->mid);
			mutex_msg->code = MUTEX_INVALID_ID;
			noza_reply(msg);
			return;
		}

		working_mutex = &si->mutex_store[mutex_msg->mid];
		if (working_mutex->token != mutex_msg->token) {
			printf("mutex token mismatch service:%d != client:%d\n", working_mutex->token, mutex_msg->token);
			mutex_msg->code = MUTEX_INVALID_TOKEN;
			noza_reply(msg);
			return;
		}
	}

	// process the request
	switch (mutex_msg->cmd) {
		case MUTEX_ACQUIRE:
			mutex_server_acquire(msg, si);
			break;

		case MUTEX_RELEASE:
			mutex_server_release(working_mutex, msg, si);
			break;

		case MUTEX_LOCK:
			mutex_server_lock(working_mutex, msg, si);
			break;

		case MUTEX_TRYLOCK:
			mutex_server_trylock(working_mutex, msg, si);
			break;

		case MUTEX_UNLOCK:
			mutex_server_unlock(working_mutex, msg, si);
			break;

		default:
			mutex_msg->code = INVALID_OP;
			noza_reply(msg);
			break;
	}
}

static void cond_server_acquire(noza_msg_t *msg, sync_info_t *si)
{
	cond_msg_t *cond_msg = (cond_msg_t *)msg->ptr;
	if (si->cond_head == NULL) {
		printf("cond: no more resource\n");
		cond_msg->code = COND_NOT_ENOUGH_RESOURCE;
		noza_reply(msg);
		return;
	}
	// update the message structure
	cond_msg->cid = si->cond_head->link.index; // assign the link and index
	cond_msg->ctoken = rand(); // assign new token
	cond_msg->code = MUTEX_SUCCESS;

	// setup the new head
	si->cond_head->ctoken = cond_msg->ctoken;
	si->cond_head->signaled = 0;
	if (si->mutex_head->pending != NULL) {
		// unlikely, handle the exception
		printf("unlikely be here....\n");
	}
	si->cond_head = (cond_item_t *)dblist_remove_head((dblink_item_t *)si->cond_head); // move head to next
	noza_reply(msg);
}

static void cond_server_release(cond_item_t *working_cond, noza_msg_t *msg, sync_info_t *si)
{
	cond_msg_t *cond_msg = (cond_msg_t *)msg->ptr;
	while (working_cond->pending) {
		pending_node_t *pending = working_cond->pending;
		cond_msg_t *cond_msg = (cond_msg_t *)pending->noza_msg.ptr;
		cond_msg->code = COND_INVALID_ID; // invalid id
		noza_reply(&pending->noza_msg);
		working_cond->pending = (pending_node_t *)dblist_remove_head(&pending->link);
	}
	working_cond->ctoken = 0; // clear access token
	working_cond->signaled = 0; // clear signal
	si->cond_head = (cond_item_t *)dblist_insert_tail((dblink_item_t *)si->cond_head, &working_cond->link);
	cond_msg->code = COND_SUCCESS; // success
	noza_reply(msg);
}

static void cond_server_wait(cond_item_t *working_cond, noza_msg_t *msg, sync_info_t *si)
{
	cond_msg_t *cond_msg = (cond_msg_t *)msg->ptr;
	if (working_cond->signaled != 0) {
		// already signaled, return immediately
		working_cond->signaled = 0;
		cond_msg->code = COND_SUCCESS;
		noza_reply(msg);
	} else {
		if (cond_msg->mid >= MAX_LOCKS) {
			printf("cond invalid id: %d\n", cond_msg->mid);
			cond_msg->code = COND_INVALID_ID;
			noza_reply(msg);
			return;
		}

		mutex_item_t *working_mutex = &si->mutex_store[cond_msg->mid];
		if (cond_msg->mtoken != working_mutex->token) {
			printf("cond invalid mutex token\n");
			cond_msg->code = COND_INVALID_TOKEN;
			noza_reply(msg);
			return;
		}

		mutex_server_unlock(working_mutex, NULL, si);
		//pending_node_t *pm = si->free_pending_head; // get a pending item from slots
		pending_node_t *pm = get_free_pending_slot(si);
		if (pm == NULL) {
			cond_msg->code = COND_NOT_ENOUGH_RESOURCE;
			printf("cond: no more free pending slot, just lock and reply\n");
			noza_reply(msg);
		} else {
			si->free_pending_head = (pending_node_t *)dblist_remove_head(
				&si->free_pending_head->link); // update the pending head
			pm->noza_msg = *msg; // copy message
			// insert into pending list
			working_cond->pending = (pending_node_t *)dblist_insert_tail(
				&working_cond->pending->link, &pm->link);
		}
	}
	return;
}

static inline pending_node_t *cond_get_pending_head(cond_item_t *working_cond)
{
	pending_node_t *head = working_cond->pending;
	working_cond->pending = (pending_node_t *)dblist_remove_head(&head->link);
	return head;
}

static void cond_server_signal(cond_item_t *working_cond, noza_msg_t *msg, sync_info_t *si)
{
	cond_msg_t *cond_msg = (cond_msg_t *)msg->ptr;
	if (working_cond->signaled == 0) {
		// enable the signal, and release one pending request, if any
		working_cond->signaled = 1;
		pending_node_t *head = cond_get_pending_head(working_cond);
		if (head) {
			// make the user mutex locked directly
			cond_msg_t *cond_msg = (cond_msg_t *)head->noza_msg.ptr;
			mutex_item_t *user_mutex = &si->mutex_store[cond_msg->mid];
			if (cond_msg->mtoken == user_mutex->token) {
				if (user_mutex->lock) {
					// insert the message to mutex pending list
					pending_node_t *pm = get_free_pending_slot(si);
					if (pm == NULL) {
						cond_msg->code = COND_NOT_ENOUGH_RESOURCE;
						printf("cond: no more free pending slot, just reply\n");
						noza_reply(msg);
						return;
					}
					pm->noza_msg = *msg; // copy message, and insert into user mutex's pending list
					mutex_insert_pending_tail(user_mutex, pm);
					return;	
				} else {
					user_mutex->lock = 1; // lock the user mutex
					cond_msg->code = COND_SUCCESS;
					noza_reply(&head->noza_msg); // and return success to the picked pending request
				}
			} else {
				printf("cond: user mutex token mismatch\n");
				cond_msg->code = COND_INVALID_TOKEN;
				noza_reply(&head->noza_msg); // and return success to the picked pending request
			}
		}
	}
	// reply success to caller
	cond_msg->code = COND_SUCCESS;
	noza_reply(msg);
}

static void cond_server_broadcast(cond_item_t *working_cond, noza_msg_t *msg, sync_info_t *si)
{
}

static void process_cond(noza_msg_t *msg, sync_info_t *si)
{
	cond_item_t *working_cond = NULL;
	cond_msg_t *cond_msg = (cond_msg_t *)msg->ptr;
	if (cond_msg->cmd != COND_ACQUIRE) {
		// sanity check
		if (cond_msg->cid >= MAX_CONDS) {
			printf("ond invalid id: %d\n", cond_msg->cid);
			cond_msg->code = COND_INVALID_ID;
			noza_reply(msg);
			return;
		}

		working_cond = &si->cond_store[cond_msg->cid];
		if (working_cond->ctoken != cond_msg->ctoken) {
			printf("cond token mismatch service:%d != client:%d\n", working_cond->ctoken, cond_msg->ctoken);
			cond_msg->code = MUTEX_INVALID_TOKEN;
			noza_reply(msg);
			return;
		}
	}

	switch (cond_msg->cmd) {
		case COND_ACQUIRE:
			cond_server_acquire(msg, si);
			break;

		case COND_RELEASE:
			cond_server_release(working_cond, msg, si);
			break;

		case COND_WAIT:
			cond_server_wait(working_cond, msg, si);
			break;

		case COND_SIGNAL:
			cond_server_signal(working_cond, msg, si);
			break;

		case COND_BROADCAST:
			cond_server_broadcast(working_cond, msg, si);
			break;

		default:
			cond_msg->code = INVALID_OP;
			noza_reply(msg);
			break;
	}
}

static void process_sem(noza_msg_t *msg, sync_info_t *si)
{
}

static void sync_init(sync_info_t *si)
{
	memset(si, 0, sizeof(si));
	DBLIST_INIT(si->mutex_store, MAX_LOCKS);
	DBLIST_INIT(si->cond_store, MAX_CONDS)
	DBLIST_INIT(si->pending_store, MAX_PENDING);
	si->mutex_head = &si->mutex_store[0];
	si->free_pending_head = &si->pending_store[0];
	si->cond_head = &si->cond_store[0];

}

static int do_synchorization_server(void *param, uint32_t pid)
{
	mutex_pid = pid;
	static sync_info_t si;

	extern uint32_t platform_get_random(void);
	srand(platform_get_random()); // for access token

	sync_init(&si);

    noza_msg_t msg;
    for (;;) {
        if (noza_recv(&msg) == 0) {
			packet_t *packet = (packet_t *)msg.ptr;
			if (packet->cmd < MUTEX_TAIL) {
				process_mutex(&msg, &si);
			} else if (packet->cmd < COND_TAIL) {
				process_cond(&msg, &si);
			} else if (packet->cmd < SEM_TAIL) {
				process_sem(&msg, &si);
			} else {
				packet->code = INVALID_OP;
				noza_reply(&msg);
			}
        }
    }

	return 0;
}

static uint8_t mutex_server_stack[1024];
void __attribute__((constructor(110))) synchorization_server_init(void *param, uint32_t pid)
{
    // TODO: move the external declaraction into a header file
    extern void noza_add_service(int (*entry)(void *param, uint32_t pid), void *stack, uint32_t stack_size);
	noza_add_service(do_synchorization_server, mutex_server_stack, sizeof(mutex_server_stack)); // TODO: add stack size
}
