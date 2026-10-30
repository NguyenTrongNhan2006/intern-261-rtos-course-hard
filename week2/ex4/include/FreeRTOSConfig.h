#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f1xx.h"
#include "tracer.h"

/* STEP 1: Priority 1..3 cho ba task; Idle dùng 0. */
#define configCPU_CLOCK_HZ                      SystemCoreClock
#define configTICK_RATE_HZ                      1000u
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configMAX_PRIORITIES                    4
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configIDLE_SHOULD_YIELD                  1

/* STEP 2: Task và đối tượng đồng bộ dùng bộ nhớ static. */
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0
#define configKERNEL_PROVIDED_STATIC_MEMORY      0
#define configMINIMAL_STACK_SIZE                128u
#define configMAX_TASK_NAME_LEN                 16

/* STEP 3: Một notification slot mỗi task. */
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configUSE_MUTEXES                       0
#define configUSE_TIMERS                        0
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskSuspend                    1

/* STEP 4: Ghi chuyển task bằng tracer có sẵn. */
#define configUSE_TRACE_FACILITY                1
#define configUSE_APPLICATION_TASK_TAG          1
#define traceTASK_SWITCHED_IN()                 tracer_on_switched_in()
#define traceTASK_SWITCHED_OUT()                tracer_on_switched_out()

/* STEP 5: Priority ngắt và handler FreeRTOS. */
#define configPRIO_BITS                         __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

/* STEP 6: Dừng tại assert hoặc lỗi tràn stack. */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW          2

#define configASSERT(x) do { \
    if (!(x)) { __disable_irq(); for (;;) { __NOP(); } } \
} while (0)

#endif