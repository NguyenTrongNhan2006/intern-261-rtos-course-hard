.syntax unified
.cpu cortex-m3
.thumb

/* ARM_CM3 port stores r4-r11 on PSP and uses LR only as a return address
 * inside PendSV. Keep the original EXC_RETURN on MSP; the native handler
 * returns here after restoring the incoming task's r4-r11. The C helpers
 * preserve those registers by AAPCS. Do not add a C wrapper/prologue that
 * restores the outgoing task's r4-r11 after the native PendSV call. */
.macro RTA_WRAPPER public, native, kind
    .section .text.\public,"ax",%progbits
    .p2align 2
    .global \public
    .type \public,%function
    .thumb_func
\public:
    push {r0, lr}
    movs r0, #\kind
    bl rta_irq_enter
    bl \native
    movs r0, #\kind
    bl rta_irq_exit
    pop {r0, lr}
    bx lr
    .size \public, .-\public
.endm

RTA_WRAPPER SysTick_Handler, rta_port_systick_handler, 0
RTA_WRAPPER PendSV_Handler, rta_port_pendsv_handler, 1

/* Fixed instruction workload: preemption preserves the loop counter in
 * the hardware exception frame. Zero is forbidden by the C caller. */
.section .text.rta_work_iterations,"ax",%progbits
.p2align 4
.global rta_work_iterations
.type rta_work_iterations,%function
.thumb_func
rta_work_iterations:
1:
    subs r0, r0, #1
    bne 1b
    bx lr
.size rta_work_iterations, .-rta_work_iterations
.section .note.GNU-stack,"",%progbits
