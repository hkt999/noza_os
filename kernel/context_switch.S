.thumb
.syntax unified

.global noza_os_trap_info

.type isr_systick, %function
.global isr_systick
isr_hardfault:
	movs r0, #255 // system call 255 --> hardfault
	b isr_svcall

isr_systick:
	movs r0, #254 // system call 0 --> reschedule

.type isr_svcall, %function
.global isr_svcall
isr_svcall:
    push {lr}
	bl noza_os_trap_info
	pop {r0}
	mov lr, r0

trap_kernel:
    // save user context
	mrs r0, psp
    subs r0, #4
    mov r1, lr
    str r1, [r0]
    subs r0, #16
    stmia r0!, {r4,r5, r6, r7}
    mov	r4, r8
	mov	r5, r9
	mov	r6, r10
	mov	r7, r11
    subs r0, #32
    stmia r0!, {r4,r5, r6, r7}
    subs r0, #16

	// load kernel state from stack
    pop {r1, r2, r3, r4, r5}
    mov r8, r1
    mov r9, r2
    mov r10, r3
    mov r11, r4
    mov r12, r5
    pop {r4, r5, r6, r7}       

	msr psr_nzcvq, ip

    pop {pc}

.global noza_os_resume_thread
noza_os_resume_thread:
	// save kernel state
	mrs ip, psr
    push {r4, r5, r6, r7, lr}
    mov r1, r8
    mov r2, r9
    mov r3, r10
    mov r4, r11
    mov r5, r12
    push {r1, r2, r3, r4, r5}    

	// load user state from stack
    // r0 is usertask_stack_start from noza_os_resume_thread
    ldmia	r0!,{r4-r7}
	mov	r8, r4
	mov	r9, r5
	mov	r10, r6
	mov	r11, r7
	ldmia	r0!,{r4-r7}
    ldmia	r0!,{r1}
    mov lr, r1
	msr psp, r0

	// return to user
	bx lr

.global init_kernel_stack
init_kernel_stack:
	mrs ip, psr
    push {r4, r5, r6, r7, lr}
    mov r1, r8
    mov r2, r9
    mov r3, r10
    mov r4, r11
    mov r5, r12
    push {r1, r2, r3, r4, r5}    

	/* switch to process stack */
	msr psp, r0
	movs r0, #2
	msr control, r0
	isb
	movs r0, #0 // system call: 0 --> reschedule
	svc 0
	bx lr


