.thumb
.syntax unified

.equ NSC_YIELD, 0
.equ NSC_SLEEP, 1
.equ NSC_THREAD_CREATE, 2
.equ NSC_THREAD_CHANGE_PRIORITY, 3
.equ NSC_THREAD_JOIN, 4
.equ NSC_THREAD_TERMINATE, 5
.equ NSC_RECV, 6
.equ NSC_REPLY, 7
.equ NSC_CALL, 8
.equ NSC_NB_RECV, 9
.equ NSC_NB_CALL, 10

.type noza_thread_yield, %function
.global noza_thread_yield
noza_thread_yield:
	push {r4-r7, lr}		// save r4-r11 and lr
	movs r0, #NSC_YIELD		// set r0 to the value of NSC_YIELD
	svc #0 					// system call, return value in r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_thread_join, %function
.global noza_thread_join
noza_thread_join:
	push {r4-r7, lr}		// save r4-r11 and lr
	mov r1, r0
	movs r0, #NSC_THREAD_JOIN	// set r0 to the value of NSC_THREAD_JOIN
	svc #0 					// system call, return value in r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_thread_sleep, %function
.global noza_thread_sleep
noza_thread_sleep:
	push {r4-r7, lr}		// save r4-r11 and lr
	mov r1, r0
	movs r0, #NSC_SLEEP		// set r0 to the value of NSC_SLEEP
	svc #0 					// system call, return value in r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_thread_change_priority, %function
.global noza_thread_change_priority
noza_thread_change_priority:
	push {r4-r7, lr}		// save r4-r11 and lr
	mov r2, r1
	mov r1, r0
	movs r0, #NSC_THREAD_CHANGE_PRIORITY	// set r0 to the value of NSC_THREAD_CHANGE_PRIORITY
	svc #0 					// system call, return value in r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_thread_terminate, %function
.global noza_thread_terminate
noza_thread_terminate:
	push {r4-r7, lr}		// save r4-r11 and lr
	movs r0, #NSC_THREAD_TERMINATE	// set r0 to the value of NSC_THREAD_TERMINATE
	svc #0 					// system call, return value in r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_recv, %function
.global noza_recv
noza_recv:
	push {r4-r7, lr}		// save r4-r11 and lr
    push {r0}          		// save the original r0 (noza_msg_t pointer) on the stack
    movs r0, #NSC_RECV 		// set r0 to the value of NSC_RECV
    svc #0             		// perform a supervisor call (SVC) with immediate value 0
    mov r4, r0         		// move the return value (in r0) to r4
    pop {r0}           		// restore the original r0 (noza_msg_t pointer) from the stack
    stmia r0!, {r1, r2, r3} // store the values of r1, r2, and r3 to consecutive memory
    mov r0, r4         		// move the return value (in r4) back to r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_reply, %function
.global noza_reply
noza_reply:
	push {r4-r7, lr}		// save r4-r11 and lr
	ldmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_REPLY		// set r0 to the value of NSC_REPLY
	svc #0 					// system call, return value in r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_call, %function
.global noza_call
noza_call:
	push {r4-r7, lr}		// save r4-r11 and lr
	push {r0}
	ldmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_CALL		// set r0 to the value of NSC_CALL
	svc #0 					// system call, return value in r0
	mov r4, r0				// move the return value (in r0) to r4
	pop {r0}				// restore the original r0 (noza_msg_t pointer) from the stack
	stmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to memory
	mov r0, r4				// move the return value (in r4) back to r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_nonblock_call, %function
.global noza_nonblock_call
noza_nonblock_call:
	push {r4-r7, lr}		// save r4-r11 and lr
	mov r4, r0
	push {r4}
	ldmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_NB_CALL	// set r0 to the value of NSC_NB_CALL
	svc #0 					// system call, return value in r0
	pop {r4}
	stmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to memory
	pop {r4-r7, pc} 		// restore r4-r11 and lr


.type noza_nonblock_recv, %function
.global noza_nonblock_recv
noza_nonblock_recv:
	push {r4-r7, lr}		// save r4-r11 and lr
	mov r4, r0
	push {r4}
	movs r0, #NSC_NB_RECV	// set r0 to the value of NSC_NB_RECV
	svc #0 					// system call, return value in r0
	pop {r4}
	stmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to memory
	pop {r4-r7, pc} 		// restore r4-r11 and lr
