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
noza_thread_join:
	mov r5, r1 					// address of retval
	mov r1, r0 					// pid
	movs r0, #NSC_THREAD_JOIN
	svc #NSC_THREAD_JOIN
	cmp r5, #0
	beq noza_thread_join_end
	str r1, [r5]				// r0 is the return value, r1 is the pid, save to retval
noza_thread_join_end:
	bx lr 						// return

.type noza_thread_detach, %function
.global noza_thread_detach
noza_thread_detach:
	mov r1, r0 					// pid
	movs r0, #NSC_THREAD_DETACH
	svc #NSC_THREAD_DETACH
	bx lr 						// return

.type noza_thread_sleep, %function
.global noza_thread_sleep
noza_thread_sleep:
	mov r2, r1
	mov r1, r0
	movs r0, #NSC_THREAD_SLEEP	// set r0 to the value of NSC_SLEEP
	svc #NSC_THREAD_SLEEP		// system call, return value in r0
	bx lr 						// return

.type noza_thread_kill, %function
.global noza_thread_kill
noza_thread_kill:
	mov r2, r1
	mov r1, r0
	movs r0, #NSC_THREAD_KILL	// set r0 to the value of NSC_KILL
	svc #NSC_THREAD_KILL
	bx lr 						// return

.type noza_thread_change_priority, %function
.global noza_thread_change_priority
noza_thread_change_priority:
	mov r2, r1
	mov r1, r0
	movs r0, #NSC_THREAD_CHANGE_PRIORITY	// set r0 to the value of NSC_THREAD_CHANGE_PRIORITY
	svc #NSC_THREAD_CHANGE_PRIORITY 		// system call, return value in r0
	bx lr 									// return

.type noza_thread_terminate, %function
.global noza_thread_terminate
noza_thread_terminate:
	mov r1, r0 						// move the terminate code to r1
	movs r0, #NSC_THREAD_TERMINATE	// set r0 to the value of NSC_THREAD_TERMINATE
	svc #NSC_THREAD_TERMINATE 		// system call, return value in r0
	bx lr 							// return

.type noza_thread_self, %function
.global noza_thread_self
noza_thread_self:
	mov r4, r0 // save parameter address (r0)
	movs r0, #NSC_THREAD_SELF	// set r0 to the value of NSC_THREAD_SELF
	svc #NSC_THREAD_SELF					// system call, return value in r0
	str r0, [r4]			// store the return value in the memory pointed by r2
	movs r0, #0				// set r0 to 0, TODO: get value from kernel return value
	bx lr 					// return

.type noza_recv, %function
.global noza_recv
noza_recv:
	mov r4, r0 // save the original r0 to r4 --> noza_msg_t pointer
	movs r0, #NSC_RECV 		// set r0 to the value of NSC_RECV
	svc #NSC_RECV             		// perform a supervisor call (SVC) with immediate value 0
	mov r5, r0 // save return value
	stmia r4!, {r1, r2, r3} // store the values of r1, r2, and r3 to consecutive memory
	mov r0, r5 // move the return value (in r4) back to r0
	bx lr

.type noza_reply, %function
.global noza_reply
noza_reply:
	ldmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_REPLY		// set r0 to the value of NSC_REPLY
	svc #NSC_REPLY			// system call, return value in r0
	bx lr

.type noza_call, %function
.global noza_call
noza_call:
	mov r4, r0				// save r0 to r4
	ldmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_CALL		// set r0 to the value of NSC_CALL
	svc #NSC_CALL 			// system call, return value in r0
	stmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to memory
	bx lr

.type noza_nonblock_call, %function
.global noza_nonblock_call
noza_nonblock_call:
	mov r4, r0				// save r0 to r4
	ldmia r0!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, load to register
	movs r0, #NSC_NB_CALL	// set r0 to the value of NSC_NB_CALL
	svc #NSC_NB_CALL 		// system call, return value in r0
	stmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to memory
	bx lr

.type noza_nonblock_recv, %function
.global noza_nonblock_recv
noza_nonblock_recv:
	mov r4, r0				// save r0 to r4
	movs r0, #NSC_NB_RECV	// set r0 to the value of NSC_NB_RECV
	svc #NSC_NB_RECV 		// system call, return value in r0
	stmia r4!, {r1, r2, r3} // r1 = pid, r2 = ptr, r3 = size, save to memory
	bx lr

.type noza_thread_create_primitive, %function
.global noza_thread_create_primitive
noza_thread_create_primitive:
	svc #NSC_THREAD_CREATE 
	bx lr
