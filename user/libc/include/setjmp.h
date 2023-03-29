#pragma once

struct _jmp_buf {
	uint32_t sp;    // Stack pointer
	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
	uint32_t lr;    // Link register
} jmp_buf;

void longjmp(jmp_buf env, int val);
int setjmp(jmp_buf env);

