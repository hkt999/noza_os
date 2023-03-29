#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "cmd_line.h"
#include "noza_console.h"

#include "pico/stdlib.h"

extern int getchar_timeout_us(uint32_t timeout_us);
extern int putchar_raw(int c);
extern void noza_thread_sleep(int ms);

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
				if ( isalnum(*p) || isalpha(*p) || (*p=='_') || (*p=='/') || (*p=='-') || (*p=='.') ) {
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
						rc->main_func(argc, argv);
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
void console_start(void *param, uint32_t pid)
{
	noza_console_init(&noza_console, "noza> ", param);
	for (;;) {
		int ch = noza_console.cmd.driver.getc(); // TODO: keep it simple, remove getc
		if (ch < 0)
			noza_thread_sleep(20);
		else
			cmd_line_putc(&noza_console.cmd, ch);
	}
}

int console_stop()
{
	noza_console_clear(&noza_console);
	return 0;
}
