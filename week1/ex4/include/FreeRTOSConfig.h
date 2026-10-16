#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f1xx.h"

#ifndef W1E4_HEAP_STAGE
#define W1E4_HEAP_STAGE 0
#endif

#ifndef W1E4_APP
#define W1E4_APP 3
#endif

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configCPU_CLOCK_HZ (SystemCoreClock)
#define configTICK_RATE_HZ 1000u
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
#define configMAX_PRIORITIES 5
#define configMINIMAL_STACK_SIZE 128u
#define configMAX_TASK_NAME_LEN 16
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configUSE_APPLICATION_TASK_TAG 1
#define configUSE_TRACE_FACILITY 1
#define configUSE_TICK_HOOK 0
#define configKERNEL_PROVIDED_STATIC_MEMORY 0

#if W1E4_HEAP_STAGE

/* Giai đoạn thu log heap */
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configCHECK_FOR_STACK_OVERFLOW 0
#define configTOTAL_HEAP_SIZE (8u * 1024u)
#define configUSE_TIMERS 0
#define configUSE_IDLE_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 1

#else

/* Firmware static cuối */
#define configSUPPORT_DYNAMIC_ALLOCATION 0
#define configSUPPORT_STATIC_ALLOCATION 1
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_TIMERS 1
#define configUSE_IDLE_HOOK 1
#define configUSE_MALLOC_FAILED_HOOK 0

#define configTIMER_TASK_PRIORITY 1u
#define configTIMER_TASK_STACK_DEPTH 128u
#define configTIMER_QUEUE_LENGTH 4u
#define INCLUDE_xTimerPendFunctionCall 0

#if W1E4_APP != 1
#include "tracer.h"
#define traceTASK_SWITCHED_IN() tracer_on_switched_in()
#define traceTASK_SWITCHED_OUT() tracer_on_switched_out()
#endif

#endif

#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetIdleTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

#define configPRIO_BITS __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8u - configPRIO_BITS))

#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#define configASSERT(condition)            \
    do {                                   \
        if (!(condition)) {                \
            __disable_irq();               \
            for (;;) { __NOP(); }          \
        }                                  \
    } while (0)

#endif