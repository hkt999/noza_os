.thumb
.syntax unified

.type app_run, %function
.global app_run, noza_thread_terminate, free_stack
app_run:
    // r0: the pointer to the info structure
    // r1: thread id

    mov r6, r1
    // load the info structure, r1 -> func_entry, r2 -> func_param, r3 -> stack_base, r4 -> stack_size
    ldmia  r0!, {r1, r2, r3, r4} 
    movs r5, #1
    str r5, [r0]    // set created flag: info->created = 1
    mov r5, r3
    add r5, r5, r4  // r5 = stack_base + stack_size --> new stack base
    mov r4, sp      // r4 = old stack pointer
    mov sp, r5      // set the stack pointer: sp = r5
    push {r3, r4}   // r3 -> stack base, r4 -> stack pointer
    mov r0, r2      // pass function parameter in r0: r0 = r2
    mov r3, r1      // mov function entry to r3
    mov r1, r6      // pass thread id in r1: r1 = r6
    blx r3          // branch to func_entry: func_entry(r0)
    mov r3, r0      // save the return value
    pop {r0, r1}    // r0 -> stack base, r1 -> stack pointer
    mov sp, r1      // restore the old stack pointer
    push {r3}       // push the return value to the stack
    bl free_stack   // free stack pointer
    pop {r0}        // pop the return value to r0
    bl noza_thread_terminate // call noza terminate to deallocate the resource
