#ifndef W2E2_FREERTOS_CONFIG_H
#define W2E2_FREERTOS_CONFIG_H

#include "stm32f1xx.h"

/* W2.2 - STEP 1: Scheduler và SysTick, mỗi tick 1 ms. */
#define configCPU_CLOCK_HZ                     (SystemCoreClock)
#define configTICK_RATE_HZ                     1000u
#define configTICK_TYPE_WIDTH_IN_BITS          TICK_TYPE_WIDTH_32_BITS
#define configUSE_PREEMPTION                   1
#define configUSE_TIME_SLICING                 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configMAX_PRIORITIES                   5
#define configIDLE_SHOULD_YIELD                 1

/* W2.2 - STEP 2: Chỉ dùng bộ nhớ static, không dùng heap. */
#define configSUPPORT_STATIC_ALLOCATION       1
#define configSUPPORT_DYNAMIC_ALLOCATION      0
#define configKERNEL_PROVIDED_STATIC_MEMORY    0
#define configMINIMAL_STACK_SIZE              128u
#define configMAX_TASK_NAME_LEN               16

/* W2.2 - STEP 3: Ring không dùng đối tượng khóa hay notification.
 * Queue thông thường vẫn hoạt động với các thiết lập này.
 */
#define configUSE_MUTEXES                     0
#define configUSE_TASK_NOTIFICATIONS          0
#define configUSE_QUEUE_SETS                  0
#define configUSE_TIMERS                      0

/* W2.2 - STEP 4: Priority ngắt 0–15; số nhỏ ưu tiên cao.
 * TIM2 dùng priority 5 để được gọi xQueueSendFromISR.
 */
#define configPRIO_BITS                       __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

/* W2.2 - STEP 5: Nối FreeRTOS port với vector exception. */
#define vPortSVCHandler                       SVC_Handler
#define xPortPendSVHandler                    PendSV_Handler
#define xPortSysTickHandler                   SysTick_Handler

/* W2.2 - STEP 6: API và hook mà ring_app.c sử dụng. */
#define INCLUDE_vTaskDelay                    1
#define configUSE_IDLE_HOOK                   0
#define configUSE_TICK_HOOK                   0
#define configUSE_MALLOC_FAILED_HOOK           0
#define configCHECK_FOR_STACK_OVERFLOW        2

/* W2.2 - STEP 7: Dừng tại lỗi để kiểm tra bằng debugger. */
#define configASSERT(condition)               \
    do {                                      \
        if (!(condition)) {                   \
            __disable_irq();                  \
            for (;;) { __NOP(); }             \
        }                                     \
    } while (0)

#endif /* W2E2_FREERTOS_CONFIG_H */