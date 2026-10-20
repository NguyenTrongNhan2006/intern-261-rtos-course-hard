#ifndef W2E1_FREERTOS_CONFIG_H
#define W2E1_FREERTOS_CONFIG_H

#include "stm32f1xx.h"

/* STEP 3: Scheduler và tick. */
#define configCPU_CLOCK_HZ                     (SystemCoreClock)
#define configTICK_RATE_HZ                     1000u // 1 tick = 1 ms.
#define configTICK_TYPE_WIDTH_IN_BITS          TICK_TYPE_WIDTH_32_BITS
#define configUSE_PREEMPTION                   1 // Task ưu tiên cao được giành CPU.
#define configUSE_TIME_SLICING                 1 // Chia lượt cho task Ready cùng priority.
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configMAX_PRIORITIES                   5 // Task priority: 0 đến 4.
#define configIDLE_SHOULD_YIELD                 1

/* STEP 4: Bộ nhớ static, Mutex và Task Notification. */
#define configSUPPORT_STATIC_ALLOCATION       1
#define configSUPPORT_DYNAMIC_ALLOCATION      0 // Không dùng FreeRTOS heap.
#define configKERNEL_PROVIDED_STATIC_MEMORY    0 // main.c cấp bộ nhớ Idle.
#define configMINIMAL_STACK_SIZE              128u // Idle: 128 words = 512 bytes.
#define configMAX_TASK_NAME_LEN               16
#define configUSE_MUTEXES                     1
#define configUSE_TASK_NOTIFICATIONS          1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES  1 // Dành index 0 cho driver I2C.

/* STEP 5: Priority ngắt; STM32F103 dùng 4 bit, giá trị 0 đến 15.
 * Số nhỏ ưu tiên cao. ISR dùng API FromISR phải có priority từ 5 đến 15.
 */
#define configPRIO_BITS                       __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

/* STEP 6: Nối FreeRTOS port với handler trong startup.c. */
#define vPortSVCHandler                       SVC_Handler
#define xPortPendSVHandler                    PendSV_Handler
#define xPortSysTickHandler                   SysTick_Handler

/* STEP 7: Dịch vụ kernel và các API cần cho task client. */
#define configUSE_TIMERS                      0 // Tắt Software Timer; SysTick vẫn chạy.
#define configUSE_IDLE_HOOK                   0
#define configUSE_TICK_HOOK                   0
#define configUSE_MALLOC_FAILED_HOOK          0
#define configCHECK_FOR_STACK_OVERFLOW        2 // Cần vApplicationStackOverflowHook.
#define INCLUDE_vTaskDelay                    1
#define INCLUDE_xTaskDelayUntil               1

/* STEP 8: Dừng tại lỗi để kiểm tra bằng debugger. */
#define configASSERT(condition)               \
    do {                                      \
        if (!(condition)) {                   \
            __disable_irq();                  \
            for (;;) { __NOP(); }             \
        }                                     \
    } while (0)

#endif /* W2E1_FREERTOS_CONFIG_H */