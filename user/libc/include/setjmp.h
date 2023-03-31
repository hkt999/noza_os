#pragma once

#include <stdint.h>

typedef uint32_t jmp_buf[11];

int		setjmp(jmp_buf env);
void	longjmp(jmp_buf env, int val);

#define _setjmp(e)		setjmp(e)
#define _longjmp(e,v)	longjmp(e, v)
