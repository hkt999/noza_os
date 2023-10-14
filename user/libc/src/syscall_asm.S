.thumb
.syntax unified

.equ NSC_THREAD_SLEEP,				0
.equ NSC_THREAD_KILL,				1
.equ NSC_THREAD_CREATE,				2
.equ NSC_THREAD_CHANGE_PRIORITY,	3
.equ NSC_THREAD_JOIN,				4
.equ NSC_THREAD_DETACH,				5
.equ NSC_THREAD_TERMINATE,			6
.equ NSC_THREAD_SELF,				7
.equ NSC_RECV,						8
.equ NSC_REPLY,						9
.equ NSC_CALL,						10
.equ NSC_NB_RECV,					11
.equ NSC_NB_CALL,					12

.type noza_thread_join, %function
.global noza_thread_join
noza_thread_join: 				// noza_thread_join(uint32_t pid, uint32_t *retval)
	push {r4-r7, lr}			// save r4-r11 and lr
	push {r1}					// push the pointer to retval to the stack
	mov r1, r0					// move the joined's pid to r1
	movs r0, #NSC_THREAD_JOIN	// set r0 to the value of NSC_THREAD_JOIN
	svc #0 						// system call, return value in r0
	pop {r3}					// pop the pointer to retval from the stack	
	cmp r3, #0					// check if the pointer to return value is 0 or not
	beq noza_thread_join_end	// if it is, jump to the end
	str r1, [r3]				// store the return value in the memory pointed by r2
noza_thread_join_end:
	pop {r4-r7, pc} 			// restore r4-r11 and lr

.type noza_thread_detach, %function
.global noza_thread_detach
noza_thread_detach:
	push {r4-r7, lr}			// save r4-r11 and lr
	mov r1, r0					// move the pid to r1
	movs r0, #NSC_THREAD_DETACH
	svc #0 						// system call, return value in r0
	pop {r4-r7, pc} 			// restore r4-r11 and lr	

.type noza_thread_sleep, %function
.global _noza_thread_sleep
_noza_thread_sleep:
	push {r4-r7, lr}			// save r4-r11 and lr
	mov r2, r1
	mov r1, r0
	movs r0, #NSC_THREAD_SLEEP	// set r0 to the value of NSC_SLEEP
	svc #0 						// system call, return value in r0
	pop {r4-r7, pc} 			// restore r4-r11 and lr

.type noza_thread_kill, %function
.global noza_thread_kill
noza_thread_kill:
	push {r4-r7, lr}			// save r4-r11 and lr
	mov r2, r1
	mov r1, r0
	movs r0, #NSC_THREAD_KILL	// set r0 to the value of NSC_KILL
	svc #0
	pop {r4-r7, pc} 			// restore r4-r11 and lr

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
	mov r1, r0 				// move the terminate code to r1
	movs r0, #NSC_THREAD_TERMINATE	// set r0 to the value of NSC_THREAD_TERMINATE
	svc #0 					// system call, return value in r0
	pop {r4-r7, pc} 		// restore r4-r11 and lr

.type noza_thread_self, %function
.global noza_thread_self
noza_thread_self:
	push {r4-r7, lr}		// save r4-r11 and lr
	push {r0}				// save parameter address (r0)	
	movs r0, #NSC_THREAD_SELF	// set r0 to the value of NSC_THREAD_SELF
	svc #0 					// system call, return value in r0
	pop {r2}
	str r0, [r2]			// store the return value in the memory pointed by r2
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
