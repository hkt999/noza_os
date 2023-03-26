.thumb
.syntax unified
.global noza_os_trap_info

// macro to save the current context (register values) to the stack
.macro save_context
    push {r4-r7}
    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    push {r4-r7}
    mov r4, r12
    push {r4}
    push {lr}
.endm

// macro to restore the saved context (register values) from the stack
.macro restore_context
    pop {r0}
    mov lr, r0
    pop {r4}
    mov r12, r4
    pop {r4-r7}
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    pop {r4-r7}
.endm

// interrupt service routine for supervisor calls (SVC)
.type isr_pendsv, %function
.global isr_pendsv, show_counter
isr_pendsv:
    b isr_systick

.type isr_svcall, %function
.global isr_svcall
isr_svcall:
    save_context
    bl noza_os_trap_info
    restore_context

// interrupt service routine for the system timer tick (SysTick) interrupt
.type isr_systick, %function
.global isr_systick
isr_systick:
    // save user context (psp stack)
	mrs r0, psp
    subs r0, #4
    mov r1, lr
    str r1, [r0]
    subs r0, #16
    stmia r0!, {r4, r5, r6, r7}
    mov	r4, r8
	mov	r5, r9
	mov	r6, r10
	mov	r7, r11
    subs r0, #32
    stmia r0!, {r4, r5, r6, r7}
    subs r0, #16

	// load kernel context (msp stack)
    pop {r1, r2, r3, r4, r5}
    mov r8, r1
    mov r9, r2
    mov r10, r3
    mov r11, r4
    mov r12, r5
    pop {r4, r5, r6, r7}       
	msr psr_nzcvq, ip
    pop {pc} // jump to kernel wihout changing priviledge level

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

// function to initialize the kernel stack and switch to the process stack
.global noza_init_kernel_stack
noza_init_kernel_stack:
    // save registers
	mrs ip, psr
    push {r4, r5, r6, r7, lr}
    mov r1, r8
    mov r2, r9
    mov r3, r10
    mov r4, r11
    mov r5, r12
    push {r1, r2, r3, r4, r5}    

	msr psp, r0
	movs r0, #2
	msr control, r0
	isb
	movs r0, #0
	svc #0 // system call:0 --> reschedule, and then swith to kernel mode
	bx lr

