.thumb
.syntax unified

.equ NSC_THREAD_SLEEP,				0
.equ NSC_THREAD_KILL,				1
.equ NSC_THREAD_CREATE,				2
.equ NSC_THREAD_CHANGE_PRIORITY,	3
.equ NSC_THREAD_JOIN,				4
.equ NSC_THREAD_DETACH,				5
.equ NSC_THREAD_TERMINATE,			6
.equ NSC_RECV,						7
.equ NSC_REPLY,						8
.equ NSC_CALL,						9
.equ NSC_NB_RECV,					10
.equ NSC_NB_CALL,					11

.type noza_thread_join, %function
.global noza_thread_join
noza_thread_join:
	push {r4-r7, lr}
	push {r1}					// address of retval
	mov r1, r0 					// pid
	movs r0, #NSC_THREAD_JOIN
	svc #0
	pop {r4}					// address of retval
	cmp r4, #0
	beq noza_thread_join_end
	str r1, [r4]				// r0 is the return value, r1 is the return code, save to retval
noza_thread_join_end:
	pop {r4-r7, pc}  

.type noza_thread_detach, %function
.global noza_thread_detach
noza_thread_detach:
	push {r4-r7, lr}
	mov r1, r0 					// pid
	movs r0, #NSC_THREAD_DETACH
	svc #0
	pop {r4-r7, pc}  


//int __noza_thread_sleep(syscall_time_t *time, syscall_time_t *remain); // in assembly
.type noza_thread_sleep, %function
.global __noza_thread_sleep
__noza_thread_sleep:
	push {r4-r7, lr}
	push {r1}					// address of remain
	ldmia r0!, {r1, r2} 		// r1 -> high 32bits, r2 -> low 32bits
	movs r0, #NSC_THREAD_SLEEP	// set r0 to the value of NSC_SLEEP
	svc #0						// system call, return value in r0, r1 -> high 32bits, r2 -> low 32bits
	pop {r3}					// address of remain
	stmia r3!, {r1, r2}			// save r1 (high 32bits), r2(low 32bits) to memory
	pop {r4-r7, pc}

.type noza_thread_kill, %function
.global noza_thread_kill
noza_thread_kill:
	push {r4-r7, lr}
	mov r2, r1
	mov r1, r0
	movs r0, #NSC_THREAD_KILL	// set r0 to the value of NSC_KILL
	svc #0
	pop {r4-r7, pc}  

.type noza_thread_change_priority, %function
.global noza_thread_change_priority
noza_thread_change_priority:
	push {r4-r7, lr}
	mov r2, r1
	mov r1, r0
	movs r0, #NSC_THREAD_CHANGE_PRIORITY	// set r0 to the value of NSC_THREAD_CHANGE_PRIORITY
	svc #0 		// system call, return value in r0
	pop {r4-r7, pc}  

.type noza_thread_terminate, %function
.global noza_thread_terminate
noza_thread_terminate:
	mov r1, r0 						// move the function return code to r1
	movs r0, #NSC_THREAD_TERMINATE	// set r0 to the value of NSC_THREAD_TERMINATE
	svc #0 							// system call, return value in r0, and never return

.type noza_recv, %function
.global noza_recv
noza_recv:
	push {r4-r7, lr}
	mov r4, r0 // save the original r0 to r4 --> noza_msg_t pointer
	movs r0, #NSC_RECV 		// set r0 to the value of NSC_RECV
	svc #0             		// perform a supervisor call (SVC) with immediate value 0
	mov r5, r0 // save return value
	stmia r4!, {r1, r2, r3} // store the values of r1, r2, and r3 to consecutive memory
	mov r0, r5 // move the return value (in r4) back to r0
	pop {r4-r7, pc}  

.type noza_reply, %function
.global noza_reply
noza_reply:
	push {r4-r7, lr}
	ldmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_REPLY		// set r0 to the value of NSC_REPLY
	svc #0					// system call, return value in r0
	pop {r4-r7, pc}  

.type noza_call, %function
.global noza_call
noza_call:
	push {r4-r7, lr}
	push {r0}				// push noza msg pointer to stack
	ldmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_CALL		// set r0 to the value of NSC_CALL
	svc #0 					// system call, return value in r0
	pop {r4}				// pop noza msg pointer from stack
	stmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to message structure
	pop {r4-r7, pc}  

.type noza_nonblock_call, %function
.global noza_nonblock_call
noza_nonblock_call:
	push {r4-r7, lr}
	mov r4, r0				// save r0 to r4
	ldmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_NB_CALL	// set r0 to the value of NSC_NB_CALL
	svc #0 					// system call, return value in r0
	stmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to memory
	pop {r4-r7, pc}  

.type noza_nonblock_recv, %function
.global noza_nonblock_recv
noza_nonblock_recv:
	push {r4-r7, lr}
	mov r4, r0				// save r0 to r4
	movs r0, #NSC_NB_RECV	// set r0 to the value of NSC_NB_RECV
	svc #0 					// system call, return value in r0
	stmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to memory
	pop {r4-r7, pc}  

.type noza_thread_create_primitive, %function
.global noza_thread_create_primitive
noza_thread_create_primitive:
	push {r4-r7, lr}
	mov r4, r0				// save r0 to r4
	push {r4}
	ldmia r4!, {r0-r3}		// load r0-r3 from memory
	svc #0  // r0 is the first parameter, don't need to setup again
	pop {r4}
	stmia r4!, {r0-r3}
	pop {r4-r7, pc}  
