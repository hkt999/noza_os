#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nozaos.h"

typedef struct {
	uint32_t cmd;
} mutex_msg_t;

#define MUTEX_ACQUIRE	1
#define MUTEX_RELEASE	2
#define MUTEX_LOCK		3
#define MUTEX_TRYLOCK	4
#define MUTEX_UNLOCK	5

static void do_mutex_server(void *param, uint32_t pid)
{
    noza_msg_t msg;
    for (;;) {
        if (noza_recv(&msg) == 0) {
			mutex_msg_t *mutex_msg = (mutex_msg_t *)msg.ptr;
			switch (mutex_msg->cmd) {
				case MUTEX_ACQUIRE:
					noza_reply(&msg);
					break;
				case MUTEX_RELEASE:
					break;
				case MUTEX_LOCK:
					break;
				case MUTEX_TRYLOCK:
					break;
				case MUTEX_UNLOCK:
					break;
				default:
					break;
			}
        }
    }
}

void __attribute__((constructor(110))) mutex_servier_init(void *param, uint32_t pid)
{
	noza_add_service(do_mutex_server);
}
