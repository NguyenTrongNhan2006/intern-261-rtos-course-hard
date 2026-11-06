#ifndef W3E2_FREERTOS_CONFIG_H
#define W3E2_FREERTOS_CONFIG_H

#include "stm32f1xx.h"
#include "tickless_rtc.h"

/* STEP 1: Dùng SysTick 1 kHz khi chạy và chuyển quyền giữ nhịp sang RTC khi idle đủ lâu. */
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       1000u
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   0
#define configMAX_PRIORITIES                     3
#define configMINIMAL_STACK_SIZE                 128u
#define configMAX_TASK_NAME_LEN                  12
#define configUSE_TICKLESS_IDLE                  2
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP   5
#define portSUPPRESS_TICKS_AND_SLEEP(xExpectedIdleTime) \
    tickless_suppress_ticks_and_sleep((uint32_t)(xExpectedIdleTime))

/* STEP 2: Cấp stack và vùng điều khiển cho cả tác vụ đo lẫn Idle bằng bộ nhớ tĩnh. */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configKERNEL_PROVIDED_STATIC_MEMORY      0
#define configUSE_TIMERS                         0
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCHECK_FOR_STACK_OVERFLOW           2
#define INCLUDE_xTaskDelayUntil                  1

/* STEP 3: Gắn vector của cổng Cortex-M3 và giữ ngưỡng ưu tiên theo các target trước. */
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
