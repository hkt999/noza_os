.thumb
.syntax unified

.type app_run, %function
.global app_run, noza_thread_terminate, free_resource, dummy_stack_addr
app_run:
    // C interface --> void app_run(thread_record_t *info, uint32_t thread_id)
    // r0: the pointer to the boot info structure
    // r1: thread_id

    // thread_record_t layout:
    // [0] reserved_vid (unused here)
    // [1] user_entry
    // [2] user_param
    // [3] stack_ptr (base)
    // [4] stack_size
    // [5] priority
    // [6] need_free_stack
    // [7] thread_data ...

    // load fields explicitly so reserved_vid at offset 0 is skipped
    ldr     r2, [r0, #4]    // user_entry
    ldr     r3, [r0, #8]    // user_param
    ldr     r4, [r0, #12]   // stack_base (stack_ptr)
    ldr     r5, [r0, #16]   // stack_size
    ldr     r7, [r0, #24]   // priority (kept for parity with old code)
    mov r6, r4          // r6 = stack_base
    add r4, r4, r5      // r3 = stack_base + stack_size --> new stack base
    mov r0, sp          // r0 = old stack pointer
    mov sp, r4          // set the new stack pointer
    push {r0, r1}       // save the old stack pointer
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
    mov sp, r2      // restore the old stack pointer --> so that we can free the stack
    mov r1, r0      // move the return value to r1
    mov r0, r3      // pass thread id in r0
    bl free_resource   // return r0 is the return code
    bl noza_thread_terminate // call noza terminate to deallocate the resource
