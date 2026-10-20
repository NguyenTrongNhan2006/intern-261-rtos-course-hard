#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
typedef int BaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef struct { int available; } StaticSemaphore_t;
#define configSUPPORT_STATIC_ALLOCATION 1
#define configUSE_MUTEXES 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 0x50u
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configASSERT(x) do { if (!(x)) { fprintf(stderr, "Assertion: %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
extern unsigned mock_critical;
#define taskENTER_CRITICAL() (++mock_critical)
#define taskEXIT_CRITICAL() (--mock_critical)
#define portYIELD_FROM_ISR(x) ((void)(x))
#endif
