#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cmd_line.h"
#include "noza_term.h"

extern void noza_thread_sleep(int ms);
extern void noza_char_init(char_driver_t *driver);
extern void noza_char_clear(char_driver_t *driver);

static void noza_term_push_line(char *cmd_str, void *user_data);

#define MAX_ARGV  12
void term_init(noza_term_t *term, const char *prompt)
{
	char_driver_t driver;
	noza_char_init(&driver);
	cmd_line_init(&term->cmd, &driver, noza_term_push_line, term);
	term->prompt = prompt;
}

static void noza_term_push_line(char *cmd_str, void *user_data)
{
	noza_term_t *term = (noza_term_t *)user_data;
	cmd_line_t *cmd = &term->cmd;
	strncpy(term->line, cmd_str, term->max_len); // copy buffer to team->line
	term->line[term->max_len-1] = 0;
	term->line_end = 1;
}

char *term_readline(noza_term_t *term, char *line, int max_len)
{
	printf("%s", term->prompt);
	term->line_end = 0;
	term->line = line;
	term->max_len = max_len;
	while (!term->line_end) {
		int ch = term->cmd.driver.getc();
		if (ch < 0)
			noza_thread_sleep(100);
		else
			cmd_line_putc(&term->cmd, ch);
	}

	return strlen(term->line) == 0 ? NULL : term->line;
}

void term_release(noza_term_t *term)
{
	noza_char_clear(&term->cmd.driver);
}
