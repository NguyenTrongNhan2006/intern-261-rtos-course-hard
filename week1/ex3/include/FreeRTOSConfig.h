#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f1xx.h"
#include "rta_measure.h"

/* Scheduler and tick. */
#define configUSE_PREEMPTION                       1
#define configUSE_TIME_SLICING                     1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    0
/* The vector points to a profiling wrapper, which calls the native port.
 * main checks these indirect routes explicitly before starting the kernel. */
#define configCHECK_HANDLER_INSTALLATION           0

#define configCPU_CLOCK_HZ                        (SystemCoreClock)
#define configTICK_RATE_HZ                        1000u
#define configTICK_TYPE_WIDTH_IN_BITS             TICK_TYPE_WIDTH_32_BITS

#define configMAX_PRIORITIES                      5
#define configIDLE_SHOULD_YIELD                    1

/* Task allocation through heap_4. */
#define configSUPPORT_DYNAMIC_ALLOCATION          1
#define configSUPPORT_STATIC_ALLOCATION           0
#define configTOTAL_HEAP_SIZE                     (8u * 1024u)
#define configMINIMAL_STACK_SIZE                  128u
#define configMAX_TASK_NAME_LEN                   16

/* Trace configuration and native scheduler hooks. */
#define configUSE_TRACE_FACILITY                  1
#define configUSE_APPLICATION_TASK_TAG            1

#define traceTASK_SWITCHED_IN()  rta_switched_in()
#define traceTASK_SWITCHED_OUT() rta_switched_out()

/* Optional features and hooks. */
#define configUSE_TIMERS                          0
#define configUSE_IDLE_HOOK                       1
#define configUSE_TICK_HOOK                       1
#define configCHECK_FOR_STACK_OVERFLOW            0
#define configUSE_MALLOC_FAILED_HOOK              1

/* Cortex-M3 interrupt priorities. */
#define configPRIO_BITS                           __NVIC_PRIO_BITS

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

/* Handler names used by the vector table. */
#define vPortSVCHandler      SVC_Handler
#define xPortPendSVHandler   rta_port_pendsv_handler
#define xPortSysTickHandler  rta_port_systick_handler

/* Task delay APIs. */
#define INCLUDE_vTaskDelay       1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_xTaskDelayUntil  1

#define configASSERT(condition)       \
    do {                              \
        if (!(condition)) {           \
            __disable_irq();          \
            for (;;) { }              \
        }                             \
    } while (0)

#endif
