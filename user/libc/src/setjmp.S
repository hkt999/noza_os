.global setjmp, longjmp

.type setjmp, %function
.type longjmp, %function

// typedef struct {
//     uint32_t sp;    // Stack pointer
//     uint32_t r4;
//     uint32_t r5;
//     uint32_t r6;
//     uint32_t r7;
//     uint32_t lr;    // Link register
// } jmp_buf;

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

setjmp:
	// save registers into _jmp_buf
    mov r1, sp          	// move the current stack pointer into r1
    str r1, [r0]        	// save the stack pointer using a low register
    add r0, r0, #4      	// adjust r0 to point to the next position in _jmp_buf

    // save r4 - r12
    stmia r0!, {r4-r7} 		// store r4-r7, lr to the _jmp_buf
    mov r1, r8
    mov r2, r9
    mov r3, r10
    stmia r0!, {r1-r3}
    mov r1, r11
    mov r2, r12
    stmia r0!, {r1-r2}
	mov r1, lr
	str r1, [r0]			// save the link register

    // return zero to indicate this is a direct call to setjmp
    mov r0, #0          	// set r0 to 0
    bx lr         

longjmp:
    // restore registers from _jmp_buf
    ldr r2, [r0]        	// load the stack pointer using a low register
    mov sp, r2          	// restore the stack pointer
    push {r1}                 // save r1 (the return value)
    add r0, r0, #4      	// adjust r0 to point to the next position in _jmp_buf

    // store r4 - r12
    ldmia r0!, {r1-r3}
    mov r4, r1
    mov r5, r2
    mov r6, r3
    ldmia r0!, {r1-r3}
    mov r7, r1
    mov r8, r2
    mov r9, r3
    ldmia r0!, {r1-r3}
    mov r10, r1
    mov r11, r2
    mov r12, r3

	ldr r2, [r0]
    mov lr, r2        		// load the link register

    // set the return value to 1
    pop {r0}

    // return
    bx lr                   // return
