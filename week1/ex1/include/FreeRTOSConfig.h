// include FreeRTOSConfig.h 
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f1xx.h"

/* Scheduling và tick. */
#define configUSE_PREEMPTION                 1
#define configUSE_TIME_SLICING               1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configCPU_CLOCK_HZ                  (SystemCoreClock)
#define configTICK_RATE_HZ                  1000u
#define configTICK_TYPE_WIDTH_IN_BITS       TICK_TYPE_WIDTH_32_BITS
#define configMAX_PRIORITIES                5
#define configIDLE_SHOULD_YIELD              1

/* Bộ nhớ: dùng heap_4 cho W1.1. */
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configSUPPORT_STATIC_ALLOCATION     0
#define configTOTAL_HEAP_SIZE               (8u * 1024u)
#define configMINIMAL_STACK_SIZE            128u
#define configMAX_TASK_NAME_LEN             16

/* Cấu hình ban đầu; chưa dùng software timer và hooks. */
#define configUSE_TIMERS                    0
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configCHECK_FOR_STACK_OVERFLOW      0
#define configUSE_MALLOC_FAILED_HOOK        0

/* Interrupt priority của Cortex-M3. */
#define configPRIO_BITS                     __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

/* Nối FreeRTOS port với tên handler trong Vector Table. */
#define vPortSVCHandler                     SVC_Handler
#define xPortPendSVHandler                  PendSV_Handler
#define xPortSysTickHandler                 SysTick_Handler

/* Các API delay dùng cho blinky. */
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskDelayUntil             1

/* Dừng tại đây khi Kernel phát hiện điều kiện không hợp lệ. */
#define configASSERT(condition)             \
    do {                                    \
        if (!(condition)) {                 \
            __disable_irq();                \
            for (;;) { }                    \
        }                                   \
    } while (0)

#endif