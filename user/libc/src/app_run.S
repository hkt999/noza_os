.thumb
.syntax unified

.type app_run, %function
.global app_run, noza_thread_terminate, free_stack
app_run:
    // r0: the pointer to the boot info structure
    // r1: thread_id

    // r2 -> func_entry, r3 -> func_param, r4 -> stack_base, r5 -> stack_size
    ldmia r0!, {r2, r3, r4, r5, r7}
    mov r6, r4      // r6 = stack_base
    add r4, r4, r5  // r3 = stack_base + stack_size --> new stack base
    mov r0, sp      // r0 = old stack pointer
    mov sp, r4      // set the new stack pointer
    push {r0, r1}   // save the old stack pointer
    push {r1, r2, r3}   // push the function entry and parameter
    mov r0, r6
    bl save_exit_context
    pop {r1, r2, r3}
    cmp r0, #0
    bne exit_entry

    // cal entry
    mov r0, r3      // pass function parameter in r0, and keep r1 as thread id
    blx r2
exit_entry:
    pop {r2, r3}    // pop old stack pointer, thread i
    mov sp, r2      // restore the old stack pointer
    push {r0}
    mov r0, r3
    bl free_stack
    pop {r0}
    bl noza_thread_terminate // call noza terminate to deallocate the resource
