#ifndef W3E3_FREERTOS_CONFIG_H
#define W3E3_FREERTOS_CONFIG_H

#include "stm32f1xx.h"

/* STEP 1: Một task gây fault ở ưu tiên thấp, SysTick 1 kHz, không tickless. */
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       1000u
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   0
#define configMAX_PRIORITIES                     3
#define configMINIMAL_STACK_SIZE                 128u
#define configMAX_TASK_NAME_LEN                  16

/* STEP 2: Cấp phát tĩnh toàn bộ, không heap. */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configKERNEL_PROVIDED_STATIC_MEMORY      0
#define configUSE_TIMERS                         0
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCHECK_FOR_STACK_OVERFLOW           2
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1

/* STEP 3: Ngưỡng BASEPRI giữ như các bài trước. HardFault và DebugMonitor (ưu tiên 0)
 * nằm trên ngưỡng nên vẫn chạy được khi kernel đang trong vùng găng. */
#define configPRIO_BITS                          __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define vPortSVCHandler                          SVC_Handler
#define xPortPendSVHandler                       PendSV_Handler
#define xPortSysTickHandler                      SysTick_Handler

#define configASSERT(condition)                  \
    do {                                         \
        if (!(condition)) {                      \
            __disable_irq();                     \
            for (;;) { __NOP(); }                \
        }                                        \
    } while (0)

#endif
