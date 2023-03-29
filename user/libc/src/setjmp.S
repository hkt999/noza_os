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

setjmp:
	// save registers into _jmp_buf
    push {r4-r7, lr}		// push R4-R7 and LR onto the stack
    mov r1, sp          	// move the current stack pointer into R1
    str r1, [r0]        	// save the stack pointer using a low register
    add r0, r0, #4      	// adjust r0 to point to the next position in _jmp_buf
    stmia r0!, {r4-r7} 		// store r4-r7, lr to the _jmp_buf
	mov r1, lr
	str r1, [r0]			// save the link register
    pop {r4-r7}     		// pop R4-R7 and LR from the stack
	pop {r0}				// pop the return address from the stack
	mov lr, r0

    // return zero to indicate this is a direct call to setjmp
    mov r0, #0          	// set R0 to 0
    bx lr         

longjmp:
    // Restore registers from _jmp_buf
    ldr r2, [r0]        	// load the stack pointer using a low register
    mov sp, r2          	// restore the stack pointer
    add r0, r0, #4      	// adjust r0 to point to the next position in _jmp_buf
    ldmia r0!, {r4-r7}  	// load R4-R7 from the _jmp_buf
	ldr r2, [r0]
    mov lr, r2        		// load the link register

    // set the return value to 1
    mov r0, r1          	// set r0 to r1 (the return value)

    // return
    bx lr               // return