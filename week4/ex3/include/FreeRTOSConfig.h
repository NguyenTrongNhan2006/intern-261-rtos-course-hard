#ifndef W4E3_FREERTOS_CONFIG_H
#define W4E3_FREERTOS_CONFIG_H

#include "stm32f1xx.h"
#include "runtime_stats.h"

void w4e3_sleep(uint32_t expected_idle_ticks);
void w4e3_imu_wait_started(void);
void w4e3_imu_wait_finished(void);
extern void *volatile w4e3_imu_waiter;
extern void *volatile w4e3_running_task;

#define traceTASK_SWITCHED_IN() (w4e3_running_task = (void *)pxCurrentTCB)
#define traceTASK_NOTIFY_TAKE_BLOCK(index) \
    do { if ((index) == 1u) { w4e3_imu_wait_started(); } } while (0)

#define traceMOVED_TASK_TO_READY_STATE(tcb) \
    do { if (w4e3_imu_waiter == (void *)(tcb)) { w4e3_imu_wait_finished(); } } while (0)

#define configCPU_CLOCK_HZ                           (SystemCoreClock)
#define configTICK_RATE_HZ                           1000u
#define configTICK_TYPE_WIDTH_IN_BITS                TICK_TYPE_WIDTH_32_BITS
#define configMAX_PRIORITIES                         10u
#define configMINIMAL_STACK_SIZE                     128u
#define configMAX_TASK_NAME_LEN                      12u
#define configUSE_PREEMPTION                         1
#define configUSE_TIME_SLICING                       0
#define configUSE_TASK_NOTIFICATIONS                 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES        3u
#define configUSE_STREAM_BUFFERS                     1
#define configSUPPORT_STATIC_ALLOCATION              1
#define configSUPPORT_DYNAMIC_ALLOCATION             0
#define configKERNEL_PROVIDED_STATIC_MEMORY          0
#define configUSE_MUTEXES                            1
#define configUSE_TIMERS                             1
#define configTIMER_TASK_PRIORITY                    1u
#define configTIMER_TASK_STACK_DEPTH                 128u
#define configTIMER_QUEUE_LENGTH                     8u
#define configCHECK_FOR_STACK_OVERFLOW                2
#define configUSE_TRACE_FACILITY                     1
#define configGENERATE_RUN_TIME_STATS                1
/* The pinned kernel requires dynamic allocation for its formatter. */
#define configUSE_STATS_FORMATTING_FUNCTIONS         0
#define configSTATS_BUFFER_MAX_LENGTH                256u
#define configRUN_TIME_COUNTER_TYPE                  uint32_t
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()     w3e1_runtime_stats_init()
#define portGET_RUN_TIME_COUNTER_VALUE()             w3e1_runtime_stats_now()
#define configUSE_TICKLESS_IDLE                      2
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP        5u
#define portSUPPRESS_TICKS_AND_SLEEP(ticks)          w4e3_sleep(ticks)
#define configUSE_IDLE_HOOK                         1
#define configUSE_TICK_HOOK                         0

#define INCLUDE_xTimerPendFunctionCall              1
#define INCLUDE_xTaskAbortDelay                     1
#define INCLUDE_vTaskPrioritySet                    1
#define INCLUDE_vTaskDelay                          1
#define INCLUDE_vTaskSuspend                        1
#define INCLUDE_uxTaskGetStackHighWaterMark          1
#define INCLUDE_xTaskGetIdleTaskHandle               1

#define configPRIO_BITS                             __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#define configASSERT(condition)                  \
    do {                                         \
        if (!(condition)) {                      \
            __disable_irq();                     \
            for (;;) { __NOP(); }                \
        }                                        \
    } while (0)

#endif
