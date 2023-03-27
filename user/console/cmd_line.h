#pragma once
#include <stdint.h>
#include "history.h"

#define BUFLEN  80
enum {
    STATE_STANDBY = 0,
    STATE_ESCAPE,
    STATE_FUNCTION1,
    STATE_FUNCTION2,
    NUM_STATE
};

typedef struct {
    int (*getc)(void);
    void (*putc)(int c);
} char_driver_t;

typedef struct cmd_line_t {
    void *user_data;
    char_driver_t driver;
    history_t history;
    int cursor;
    int len;
    char working_buffer[BUFLEN];
    void (*process_command)(char *cmd_str, void *user_data);
    void (*state_func)(struct cmd_line_t *obj, int c);
} cmd_line_t;

void cmd_line_init(cmd_line_t *cmd, char_driver_t *driver, void (*process_command)(char *cmd_str, void *user_data), void *user_data);
void cmd_line_putc(cmd_line_t *edit, int c);