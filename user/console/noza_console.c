#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "pico/stdio.h"
#include "noza_irq_defs.h"
#include "service/irq/irq_client.h"

#include "cmd_line.h"
#include "noza_console.h"
#include "nozaos.h"
#include "nz_stdlib.h"
#include "noza_fs.h"

#define MAX_BUILTIN_CMDS  32
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
			} else if (strncmp(argv[0], "ls", 32) == 0) {
				const char *path = (argc > 1) ? argv[1] : ".";
				int dir = noza_opendir(path);
				if (dir < 0) {
					printf("ls: cannot open %s\n", path);
				} else {
					noza_fs_dirent_t ent;
					int at_end = 0;
					while (noza_readdir(dir, &ent, &at_end) == 0 && !at_end) {
						printf("%s\n", ent.name);
					}
					noza_closedir(dir);
				}
			} else if (strncmp(argv[0], "cat", 32) == 0) {
				if (argc < 2) {
					printf("cat: missing file\n");
				} else {
					int fd = noza_open(argv[1], 0, 0);
					if (fd < 0) {
						printf("cat: cannot open %s\n", argv[1]);
					} else {
						char buf[128];
						int r;
						while ((r = noza_read(fd, buf, sizeof(buf))) > 0) {
							fwrite(buf, 1, r, stdout);
						}
						noza_close(fd);
					}
				}
			} else if (strncmp(argv[0], "mkdir", 32) == 0) {
				if (argc < 2) {
					printf("mkdir: missing path\n");
				} else if (noza_mkdir(argv[1], 0755) != 0) {
					printf("mkdir: failed %s\n", argv[1]);
				}
			} else if (strncmp(argv[0], "rm", 32) == 0) {
				if (argc < 2) {
					printf("rm: missing path\n");
				} else if (noza_unlink(argv[1]) != 0) {
					printf("rm: failed %s\n", argv[1]);
				}
			} else if (strncmp(argv[0], "cd", 32) == 0) {
				if (argc < 2) {
					printf("cd: missing path\n");
				} else if (noza_chdir(argv[1]) != 0) {
					printf("cd: failed %s\n", argv[1]);
				}
			} else if (strncmp(argv[0], "pwd", 32) == 0) {
				char buf[NOZA_FS_MAX_PATH];
				if (noza_getcwd(buf, sizeof(buf))) {
					printf("%s\n", buf);
				} else {
					printf("pwd: failed\n");
				}
			} else  {
				while (rc->name && rc->main_func) {
					if (strncmp(rc->name, argv[0], 32) == 0) {
							uint32_t stack_size = rc->stack_size;
							if (strcmp(argv[argc-1], "&") == 0) {
								argc--;
								argv[argc] = NULL;
								int ret = noza_process_exec_detached_with_stack(rc->main_func, argc, argv, stack_size);
								if (ret != 0) {
									printf("%s: launch failed (%d)\n", rc->name, ret);
								}
							} else {
								int exit_code = 0;
								int ret = noza_process_exec_with_stack(rc->main_func, argc, argv, &exit_code, stack_size);
								if (ret != 0) {
									printf("%s: exec failed (%d)\n", rc->name, ret);
								}
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

	int ret = irq_service_subscribe(NOZA_IRQ_UART0);
	if (ret != 0) {
		// fallback: keep old polling behavior
		for (;;) {
			int ch = noza_console.cmd.driver.getc();
			if (ch < 0)
				noza_thread_sleep_ms(50, NULL);
			else {
				cmd_line_putc(&noza_console.cmd, ch);
			}
		}
	} else {
		noza_msg_t msg;
		for (;;) {
			if (noza_recv(&msg) != 0) {
				continue;
			}
			if (msg.ptr && msg.size == sizeof(noza_irq_event_t)) {
				noza_irq_event_t evt = *(noza_irq_event_t *)msg.ptr;
				noza_reply(&msg); // unmask
				if (evt.irq_id == NOZA_IRQ_UART0) {
					for (;;) {
						int ch = noza_console.cmd.driver.getc();
						if (ch < 0) {
							break;
						}
						cmd_line_putc(&noza_console.cmd, ch);
					}
				}
			} else {
				noza_reply(&msg);
			}
		}
	}
	return 0;
}

int console_stop()
{
	noza_console_clear(&noza_console);
	return 0;
}
