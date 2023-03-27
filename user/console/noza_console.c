#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cmd_line.h"
#include "console.h"

#include "pico/stdlib.h"

#define NOZA_LIB

#ifdef HOST_TEST
#include <unistd.h>
#define getchar_timeout_us(t)	getc(stdin)
#define putchar_raw(c) putc(c, stdout)
#define noza_thread_sleep(t) usleep(t*1000)
#endif

#ifdef SA_TEST
extern int getchar_timeout_us(uint32_t timeout_us);
extern int putchar_raw(int c);
#define  noza_thread_sleep(t) sleep_ms(t)
#endif

#ifdef NOZA_LIB
extern int getchar_timeout_us(uint32_t timeout_us);
extern int putchar_raw(int c);
extern void noza_thread_sleep(int ms);
#endif

static void noza_char_putc(int c)
{
    if (c=='\n') {
        putchar_raw('\r');
    }
    putchar_raw(c);
}

static int noza_char_getc()
{
    int c = getchar_timeout_us(0);
    if (c < 0) {
        noza_thread_sleep(10);
    }

    return c;
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

#define MAX_NUM_CMD	12
typedef struct {
	callback_t cmd_func;
	const char *name;
} register_command_t;

typedef struct {
	cmd_line_t cmd;
	const char *prompt;
	register_command_t cmd_list[MAX_NUM_CMD];
	int num_cmd;
	void *user_data;
} noza_console_t;

static void noza_console_process_command(char *cmd_str, void *user_data);
void noza_console_init(noza_console_t *console, const char *prompt, void *user_data)
{
	char_driver_t driver;
	noza_char_init(&driver);
	bzero(console->cmd_list, sizeof(console->cmd_list));
	cmd_line_init(&console->cmd, &driver, noza_console_process_command, console);
	console->prompt = prompt;
	console->user_data = user_data;
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
			for (i=0; i<MAX_NUM_CMD; i++) {
				if (console->cmd_list[i].name && (strncmp(console->cmd_list[i].name, argv[0], 32) == 0)) {
					console->cmd_list[i].cmd_func(argc, argv, console->user_data);
					break;
				}
			}
			if (i==MAX_NUM_CMD)
				printf("command not found: %s\n", argv[0]);
		}
		free(buf);
	}
	printf("%s", console->prompt);
}

int noza_console_register(noza_console_t *console, const char *name, callback_t func)
{
	for (int i=0; i<MAX_NUM_CMD; i++) {
		if (console->cmd_list[i].cmd_func == NULL) {
			console->cmd_list[i].cmd_func = func;
			console->cmd_list[i].name = name;
			return 0; // success
		}
	}

	return -1;
}

int noza_console_unregister(noza_console_t *console, const char *name)
{
	for (int i=0; i<MAX_NUM_CMD; i++) {
		if (strncmp(console->cmd_list[i].name, name, 32) == 0) {
			console->cmd_list[i].cmd_func = NULL;
			console->cmd_list[i].name = NULL;
			return 0; // success
		}
	}

	return -1;
}

// global console

static noza_console_t noza_console;
int console_start()
{
	noza_console_init(&noza_console, "noza> ", NULL);
	for (;;) {
		int ch = noza_console.cmd.driver.getc(); // TODO: keep it simple, remove getc
		if (ch < 0)
			continue;
		cmd_line_putc(&noza_console.cmd, ch);
	}
	return 0;
}

int console_stop()
{
	noza_console_clear(&noza_console);
	return 0;
}

int console_register(const char *name, callback_t func)
{
	return noza_console_register(&noza_console, name, func);
}

int console_unregister(const char *name)
{
	return noza_console_unregister(&noza_console, name);
}

#ifndef NOZA_LIB
int main()
{
    stdio_init_all();

	for (int i=0; i<4; i++) {
		printf("count: %d\n", i);
		sleep_ms(1000);
	}
	console_start();
}
#endif
