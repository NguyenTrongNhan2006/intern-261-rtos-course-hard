#ifndef W3E1_FREERTOS_CONFIG_H
#define W3E1_FREERTOS_CONFIG_H

#include "stm32f1xx.h"
#include "runtime_stats.h"

/* STEP 1: Cho phép Parser giành quyền chạy ngay khi sẵn sàng. */
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       1000u
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   0
#define configMAX_PRIORITIES                     3
#define configMINIMAL_STACK_SIZE                 128u
#define configMAX_TASK_NAME_LEN                  12

/* STEP 2: Ứng dụng cấp phát tĩnh các đối tượng FreeRTOS và stack của task. */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configKERNEL_PROVIDED_STATIC_MEMORY      0
#define configUSE_TASK_NOTIFICATIONS             1
#define configUSE_STREAM_BUFFERS                 1
#define configUSE_TIMERS                         0
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCHECK_FOR_STACK_OVERFLOW           2
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_xTaskGetIdleTaskHandle           1

/* STEP 3: Đặt ngưỡng BASEPRI để vùng găng che được cả hai ngắt nhận. */
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

/* STEP 4: Lấy bộ đếm run-time stats theo task, dùng TIM2 thay cho SysTick. */
#define configUSE_TRACE_FACILITY                 1
#define configGENERATE_RUN_TIME_STATS            1
#define configRUN_TIME_COUNTER_TYPE              uint32_t
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() w3e1_runtime_stats_init()
#define portGET_RUN_TIME_COUNTER_VALUE()         w3e1_runtime_stats_now()

#define configASSERT(condition)                  \
    do {                                         \
        if (!(condition)) {                      \
            __disable_irq();                     \
            for (;;) { __NOP(); }                \
        }                                        \
    } while (0)

#endif
