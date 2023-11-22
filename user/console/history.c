#include <string.h>
#include "history.h"
#include "nz_stdlib.h"

void history_init(history_t *h)
{
	h->count = 0;
	h->iter = 0;
	history_new_line(h);
}

void history_new_line(history_t *h)
{
	if (h->count==0) {
		h->commands[0] = strdup("");
		h->count++;
	} else {
		if (h->commands[h->count-1][0] == 0) {
			// top is already empty, do nothing
			return;
		} else {
			// push empty line to the top
			if (h->count >= HISTORY_SIZE-1) {
				int i;
				// full, scroll history, free the oldest
				free(h->commands[0]);
				for (i=0; i<HISTORY_SIZE-1; i++) {
					h->commands[i] = h->commands[i+1];
				}
				h->commands[i] = strdup("");
			} else {
				// not full, just push empty line to the top
				h->commands[h->count++] = strdup("");
			}
		}
	}
}

void history_save(history_t *h, char *p)
{
	if (h->count == 0) {
		h->commands[0] = strdup(p);
		h->count++;
		return;
	}
	if (h->iter < h->count) {
		free(h->commands[h->iter]);
		h->commands[h->iter] = strdup(p);
	}
}

void history_load(history_t *h, char **dest)
{
	if (h->iter < h->count) {
		*dest = h->commands[h->iter];
		return;
	}
	dest = NULL;
}

void history_backward(history_t *h)
{
	if (h->iter > 0) {
		h->iter--;
	}
}

void history_forward(history_t *h)
{
	if (h->iter < h->count) {
		h->iter++;
	}
}