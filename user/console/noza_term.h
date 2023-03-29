#pragma once

#include "cmd_line.h"
typedef struct {
	cmd_line_t 			cmd;
	const char 			*prompt;
	int					num_cmd;
	int					line_end;
	int					max_len;
	char				*line;
} noza_term_t;

void	term_init(noza_term_t *term, const char *prompt);
char *	term_readline(noza_term_t *term, char *line, int max_len);
void	term_release(noza_term_t *term);
