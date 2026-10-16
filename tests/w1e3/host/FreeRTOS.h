#ifndef HOST_FREERTOS_H
#define HOST_FREERTOS_H
#include <stdint.h>
#include <stdlib.h>
typedef int BaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef struct { uint32_t CTRL, CYCCNT; } host_dwt_t;
typedef struct { uint32_t DEMCR; } host_debug_t;
extern host_dwt_t host_dwt;
extern host_debug_t host_debug;
extern uint32_t host_primask, SystemCoreClock;
#define DWT (&host_dwt)
#define CoreDebug (&host_debug)
#define DWT_CTRL_NOCYCCNT_Msk (1u << 25)
#define DWT_CTRL_CYCCNTENA_Msk 1u
#define CoreDebug_DEMCR_TRCENA_Msk (1u << 24)
#define configASSERT(x) do { if (!(x)) abort(); } while (0)
#define pdMS_TO_TICKS(x) (x)
#define __DSB() ((void)0)
#define __ISB() ((void)0)
#define __get_PRIMASK() (host_primask)
#define __disable_irq() (host_primask = 1u)
#define __set_PRIMASK(x) (host_primask = (x))
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
uint32_t xPortGetFreeHeapSize(void);
uint32_t xPortGetMinimumEverFreeHeapSize(void);
#endif
