#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "cmd_line.h"
#include "noza_console.h"
#include "nozaos.h"
#include "nz_stdlib.h"

#define MAX_BUILTIN_CMDS  16
typedef struct {
    int count;
    builtin_cmd_t builtin_cmds[MAX_BUILTIN_CMDS];
} builtin_table_t;

static builtin_table_t cmd_table;
void console_add_command(const char *name, int (*main)(int argc, char **argv), const char *desc, uint32_t stack_size)
{
	builtin_table_t *table = &cmd_table;
	static int inited = 0;
	if (!inited) {
		memset(table, 0, sizeof(cmd_table));
		inited = 1;
	}

    if (table->count >= MAX_BUILTIN_CMDS) {
        return; // fail
    }
	builtin_cmd_t *cmd = &table->builtin_cmds[table->count];
    cmd->name = name;
    cmd->main_func = main;
    cmd->help_msg = desc;
	cmd->stack_size = stack_size;
    table->count++;
}

extern int getchar_timeout_us(uint32_t timeout_us);
extern int putchar_raw(int c);

static void noza_char_putc(int c)
{
    if (c=='\n') {
        putchar_raw('\r');
    }
    putchar_raw(c);
}

static int noza_char_getc()
{
    return getchar_timeout_us(0);
}

void noza_char_init(char_driver_t *driver)
{
    driver->putc = noza_char_putc;
    driver->getc = noza_char_getc;
}

void noza_char_clear(char_driver_t *driver)
{
    driver->putc = NULL;
    driver->getc = NULL;
}

////

typedef struct {
	cmd_line_t 			cmd;
	const char 			*prompt;
	int					num_cmd;
	builtin_cmd_t 		*table;
} noza_console_t;

static void noza_console_process_command(char *cmd_str, void *user_data);
void noza_console_init(noza_console_t *console, const char *prompt, builtin_cmd_t *command_table)
{
	char_driver_t driver;
	noza_char_init(&driver);
	cmd_line_init(&console->cmd, &driver, noza_console_process_command, console);
	console->prompt = prompt;
	console->table = command_table;
}

void noza_console_clear(noza_console_t *console)
{
	noza_char_clear(&console->cmd.driver);
}

static void parse_input_command(char *cmdbuf, int *argc, char **argv, int max_argv)
{
	enum {
		STATE_NEW_TOKEN = 0,
		STATE_PROCESSING
	};
	memset(argv, 0, sizeof(const char *) * max_argv);
	*argc = 0;

	int state = STATE_NEW_TOKEN;
	char *p = cmdbuf;
	while (*p != 0) {
		switch (state) {
			case STATE_NEW_TOKEN:
				if ( isalnum(*p) || isalpha(*p) || (*p=='_') || (*p=='/') || (*p=='-') || (*p=='.') || (*p=='&')) {
					argv[(*argc)++] = p;
					state = STATE_PROCESSING;
				} 
				break;

			case STATE_PROCESSING:
				if (isalnum(*p) || (*p=='_') || (*p=='/') || (*p=='-') || (*p=='.')) {
					break; // keep the current state
				} else {
					*p = (char)0; // terminate
					state = STATE_NEW_TOKEN;
				}
				break;
		}
		p++;
	}
}

typedef struct {
	int argc;
	char **argv;
	int (*main_func)(int argc, char **argv);
} run_t;

#if 0
static int run_entry(void *param, uint32_t pid)
{
	run_t *run = (run_t *)param;
	return run->main_func(run->argc, run->argv);
}
#endif

#define MAX_ARGV  12
static void noza_console_process_command(char *cmd_str, void *user_data)
{
	noza_console_t *console = (noza_console_t *)user_data;
	cmd_line_t *cmd = &console->cmd;
	char *argv[MAX_ARGV];
	int i, argc;

	if (strlen(cmd->working_buffer) > 0) {
		char *buf = strdup(cmd->working_buffer);
		parse_input_command(buf, &argc, argv, MAX_ARGV);
		if (argc > 0) {
			builtin_cmd_t *rc = console->table;
			if (strncmp(argv[0], "help", 32) == 0) {
				printf("help - show this message\n");
				while (rc->name && rc->main_func) {
					printf("%s - %s\n", rc->name, rc->help_msg);
					rc++;
				}
			} else  {
				while (rc->name && rc->main_func) {
					if (strncmp(rc->name, argv[0], 32) == 0) {
						if (strcmp(argv[argc-1], "&") == 0) {
							argc--;
							argv[argc] = NULL;
							noza_process_exec_detached(rc->main_func, argc, argv);
						} else {
							noza_process_exec(rc->main_func, argc, argv, NULL);
						}
						break;
					}
					rc++;
				}
				if (rc->name == NULL)
					printf("command not found: %s\n", argv[0]);
			}
		}
		free(buf);
	}
	printf("%s", console->prompt);
}

// global console

static noza_console_t noza_console;
int console_start()
{ 
	noza_console_init(&noza_console, "noza> ", &cmd_table.builtin_cmds[0]);
	for (;;) {
		int ch = noza_console.cmd.driver.getc();
		if (ch < 0)
			noza_thread_sleep_ms(50, NULL);
		else {
			cmd_line_putc(&noza_console.cmd, ch);
		}
	}
	return 0;
}

int console_stop()
{
	noza_console_clear(&noza_console);
	return 0;
}
