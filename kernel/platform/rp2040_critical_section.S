.thumb
.syntax unified
.global ENTER_CRITICAL
.global LEAVE_CRITICAL

ENTER_CRITICAL:
    cpsid i // disable interrupts by setting PRIMASK
    bx lr

LEAVE_CRITICAL:
    cpsie i // enable interupts by clearing PRIMASK
    bx lr
