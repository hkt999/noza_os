.thumb
.syntax unified

.type noza_syscall, %function
.global noza_syscall
noza_syscall:
	svc 0
	bx lr

