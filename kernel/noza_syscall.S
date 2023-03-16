.thumb
.syntax unified

.type asm_noza_syscall, %function
.global asm_noza_syscall
asm_noza_syscall:
	svc 0
	bx lr

