#include <string.h>
#include "history.h"
#include "cmd_line.h"

#define KEY_TAB         9
#define KEY_CR          10
#define KEY_LF          13  
#define KEY_ESC         27
#define KEY_BACKSPACE   8
#define KEY_PAGEUP      53
#define KEY_PAGEDOWN    54
#define KEY_UP          65
#define KEY_DOWN        66
#define KEY_RIGHT       67
#define KEY_LEFT        68
#define KEY_DEL         51
#define KEY_VDEL        126

static void state_escape(cmd_line_t *edit, int c);
static void state_func1(cmd_line_t *edit, int c);
static void state_func2(cmd_line_t *edit, int c);
static void state_stand_by(cmd_line_t *edit, int c);

static void cmd_line_key_delete(cmd_line_t *edit);
static void cmd_line_key_backspace(cmd_line_t *edit);
static void cmd_line_key_left(cmd_line_t *edit);
static void cmd_line_key_right(cmd_line_t *edit);;
static void cmd_line_update(cmd_line_t *edit, char *line);
static void cmd_line_key_up(cmd_line_t *edit);
static void cmd_line_key_down(cmd_line_t *edit);
static void cmd_line_key_tab(cmd_line_t *edit);
static void cmd_line_key_page_up(cmd_line_t *edit);
static void cmd_line_key_page_down(cmd_line_t *edit);
static void cmd_line_insert(cmd_line_t *edit, int c);
static void cmd_line_push_line(cmd_line_t *edit);

// implementation

static void cmd_line_key_delete(cmd_line_t *edit)
{
    if (edit->cursor < edit->len) {
        for (int i=edit->cursor; i<edit->len-1; i++) {
            edit->working_buffer[i] = edit->working_buffer[i+1];
        }
        edit->len--;
        for (int i=edit->cursor; i<edit->len; i++) {
            edit->driver.putc(edit->working_buffer[i]);
        }
        edit->driver.putc(' '); // clear the tail
        for (int i=edit->cursor; i<edit->len+1; i++) {
            edit->driver.putc('\b');
        }
    }
}

static void cmd_line_key_backspace(cmd_line_t *edit)
{
    if (edit->cursor>0) {
        cmd_line_key_left(edit);
        cmd_line_key_delete(edit);
    }
}

static void cmd_line_key_left(cmd_line_t *edit)
{
    if (edit->cursor > 0) {
        edit->cursor--;
        edit->driver.putc('\b');
    }
}

static void cmd_line_key_right(cmd_line_t *edit)
{
    if (edit->cursor < edit->len) {
        edit->driver.putc(edit->working_buffer[edit->cursor]);
        edit->cursor++;
    }
}

static void state_stand_by(cmd_line_t *edit, int c)
{
    switch (c) {
        case KEY_TAB:
            cmd_line_key_tab(edit);
            break;

        case KEY_CR:
        case KEY_LF:
            cmd_line_push_line(edit);
            break;

        case KEY_ESC:
            edit->state_func = state_escape;
            break;

        case KEY_BACKSPACE:
        case -1:
            cmd_line_key_backspace(edit);
            break;

        default:
            cmd_line_insert(edit, c);
            break;
    }
}

static void state_escape(cmd_line_t *edit, int c)
{
    switch (c) {
        case 91: // [
            edit->state_func = state_func1;
            break;

        default:
            edit->state_func = state_stand_by;
            break;
    }
}

static void state_func1(cmd_line_t *edit, int c)
{
    switch (c) {
        case KEY_PAGEUP:
            cmd_line_key_page_up(edit);
            break;

        case KEY_PAGEDOWN:
            cmd_line_key_page_down(edit);
            break;

        case KEY_UP:
            cmd_line_key_up(edit);
            edit->state_func = state_stand_by;
            break;

        case KEY_DOWN:
            cmd_line_key_down(edit);
            edit->state_func = state_stand_by;
            break;

        case KEY_RIGHT:
            cmd_line_key_right(edit);
            edit->state_func = state_stand_by;
            break;

        case KEY_LEFT:
            cmd_line_key_left(edit);
            edit->state_func = state_stand_by;
            break;

        case KEY_DEL: 
            edit->state_func = state_func2;
            break;
    }
}

static void state_func2(cmd_line_t *edit, int c)
{
    switch (c) {
        case KEY_VDEL:
            cmd_line_key_delete(edit);
            edit->state_func = state_stand_by;
            break;
    }
}


static void cmd_line_clear(cmd_line_t *edit)
{
    for (int i=0; i<edit->len; i++)
        edit->driver.putc('\b');
    for (int i=0; i<edit->len; i++)
        edit->driver.putc(' '); // clear the tail
    for (int i=0; i<edit->len; i++)
        edit->driver.putc('\b');
    edit->len = edit->cursor = strlen(edit->working_buffer);
}

static void cmd_line_update(cmd_line_t *edit, char *line)
{
    cmd_line_clear(edit);
    strncpy(edit->working_buffer, line, sizeof(edit->working_buffer));
    edit->len = edit->cursor = strlen(edit->working_buffer);
    for (int i=0; i<edit->len; i++) {
        edit->driver.putc(edit->working_buffer[i]);
    }
}

static void cmd_line_key_up(cmd_line_t *edit)
{
    char *line;
    if (edit->len > 0) {
        edit->working_buffer[edit->len] = 0;
        history_save(&edit->history, edit->working_buffer);
    }
    history_backward(&edit->history);
    history_load(&edit->history, &line);
    cmd_line_update(edit, line);
}

static void cmd_line_key_down(cmd_line_t *edit)
{
    char *line;
    if (edit->len > 0) {
        edit->working_buffer[edit->len] = 0;
        history_save(&edit->history, edit->working_buffer);
    }
    history_forward(&edit->history);
    history_load(&edit->history, &line);
    cmd_line_update(edit, line);
}

static void cmd_line_key_tab(cmd_line_t *edit)
{
    /* do nothing here */
}

static void cmd_line_key_page_up(cmd_line_t *edit)
{
    /* do nothing here */
}

static void cmd_line_key_page_down(cmd_line_t *edit)
{
    /* do nothing here */
}

static void cmd_line_insert(cmd_line_t *edit, int c)
{
    if (edit->len >= (int)(sizeof(edit->working_buffer)))
        return;

    for (int i=edit->len; i>edit->cursor; i--) {
        edit->working_buffer[i] = edit->working_buffer[i-1];
    }

    edit->working_buffer[edit->cursor++] = c;
    edit->len++;

    for (int i=edit->cursor-1; i<edit->len; i++)
        edit->driver.putc(edit->working_buffer[i]);

    for (int i=edit->cursor; i<edit->len; i++)
        edit->driver.putc('\b');
}

static void cmd_line_push_line(cmd_line_t *edit)
{
    edit->working_buffer[edit->len] = 0;
    if (edit->len) {
        history_new_line(&edit->history);
        history_save(&edit->history, edit->working_buffer);
        edit->driver.putc('\n');
        edit->process_command(edit->working_buffer, edit->user_data);
        bzero(edit->working_buffer, sizeof(edit->working_buffer));
        edit->cursor = edit->len = 0;
        history_new_line(&edit->history);
    } else {
        edit->driver.putc('\n');
        edit->process_command(edit->working_buffer, edit->user_data);
    }
}

// exported functions
void cmd_line_init(cmd_line_t *edit, char_driver_t *driver, void (*process_command)(char *cmd_str, void *), void *user_data)
{
    edit->cursor = 0;
    edit->len = 0;
    edit->state_func = state_stand_by;
    history_new_line(&edit->history);
    edit->driver.getc = driver->getc;
    edit->driver.putc = driver->putc;
    edit->process_command = process_command;
    edit->user_data = user_data;
    bzero(edit->working_buffer, sizeof(edit->working_buffer));
}

void cmd_line_putc(cmd_line_t *edit, int c)
{
    edit->state_func(edit, c);
}

char *cmd_line_get_line(cmd_line_t *edit)
{

}