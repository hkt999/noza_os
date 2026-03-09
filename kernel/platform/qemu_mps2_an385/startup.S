.syntax unified
.cpu cortex-m3
.thumb

.extern main
.extern __libc_init_array
.extern isr_hardfault
.extern isr_svcall
.extern isr_pendsv
.extern isr_systick

.global __isr_vector
.global Reset_Handler
.global Default_Handler

.section .isr_vector,"a",%progbits
.type __isr_vector, %object
__isr_vector:
    .word __stack_top
    .word Reset_Handler
    .word Default_Handler
    .word isr_hardfault
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word isr_svcall
    .word Default_Handler
    .word 0
    .word isr_pendsv
    .word isr_systick
    .rept 32
    .word Default_Handler
    .endr
.size __isr_vector, . - __isr_vector

.section .text.Reset_Handler,"ax",%progbits
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
3:
    cmp r0, r1
    bcs 4f
    str r2, [r0], #4
    b 3b
4:
    bl __libc_init_array
    bl main
5:
    b 5b
.size Reset_Handler, . - Reset_Handler

.section .text.Default_Handler,"ax",%progbits
.type Default_Handler, %function
.thumb_func
Default_Handler:
    b .
.size Default_Handler, . - Default_Handler
