#pragma once

#define HISTORY_SIZE     10
#define HISTORY_LINE_MAX 80

typedef struct {
	char commands[HISTORY_SIZE][HISTORY_LINE_MAX];
	int count;
	int iter;
} history_t;

void history_init(history_t *h);
void history_new_line(history_t *h);
void history_save(history_t *h, char *p);
void history_load(history_t *h, char **dst);
void history_backward(history_t *h);
void history_forward(history_t *h);
