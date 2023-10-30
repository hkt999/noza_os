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

// interrupt service routine for hardfault
.type isr_hardfault, %function
.global isr_hardfault
isr_hardfault:
    save_context
    movs r0, #255 // hardfault
    bl noza_os_trap_info
    restore_context
    bx lr

// interrupt service routine for supervisor calls (SVC)
.type isr_pendsv, %function
.global isr_pendsv
isr_pendsv: // for context switching in an OS
    b isr_systick // do timer context switch

.type isr_svcall, %function 
.global isr_svcall
.global show_svc
.global show_registers
isr_svcall: // for system calls
    // get svc call number
    push {r4}
    mrs r4, psp
    ldmia r4!, {r0-r3} // get r0-r3 from user stack
    pop {r4}

    // sanity check
    cmp r0, #12
    ble go_service
    cmp r0, #255
    beq go_service
    bx lr

go_service:
    push {r0-r3} // push the SVC call parameters (r0-r3)
    // save user context (psp stack)
	mrs r0, psp
    subs r0, #4                     // 1 register * 4 bytes
    mov r1, lr                      // save LR
    str r1, [r0]                    // save LR to stack
    subs r0, #16                    // 4 registers * 4 bytes
    stmia r0!, {r4, r5, r6, r7}     // save r4-r7 to stack
    mov	r4, r8                  
	mov	r5, r9
	mov	r6, r10
	mov	r7, r11
    subs r0, #32                    // reserve space for 8 registers
    stmia r0!, {r4, r5, r6, r7}     // save r8-r11 to stack (4 registers)
    subs r0, #16                    // come back to the stack pointer

    // make kernel save the system call parameters (r0-r3)
    mov r4, r0
    pop {r0-r3}
    push {lr}
    push {r4}
    bl noza_os_trap_info
    pop {r0}
    pop {r4}
    mov lr, r4
    
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

// interrupt service routine for the system timer tick (SysTick) interrupt
.type isr_systick, %function
.global isr_systick
isr_systick:
    // save user context (psp stack)
	mrs r0, psp
    subs r0, #4                     // 1 register * 4 bytes
    mov r1, lr                      // save LR
    str r1, [r0]                    // save LR to stack
    subs r0, #16                    // 4 registers * 4 bytes
    stmia r0!, {r4, r5, r6, r7}     // save r4-r7 to stack
    mov	r4, r8                  
	mov	r5, r9
	mov	r6, r10
	mov	r7, r11
    subs r0, #32                    // reserve space for 8 registers
    stmia r0!, {r4, r5, r6, r7}     // save r8-r11 to stack (4 registers)
    subs r0, #16                    // come back to the stack pointer

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
.global test_addr
noza_os_resume_thread:
	mrs ip, psr
    push {r4, r5, r6, r7, lr}
    mov r1, r8
    mov r2, r9
    mov r3, r10
    mov r4, r11
    mov r5, r12
    push {r1-r5}

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
	msr psp, r0 // load user stack pointer (r0) to user stack pointer (psp)
	bx lr // return to user with r0 as stack pointer

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
