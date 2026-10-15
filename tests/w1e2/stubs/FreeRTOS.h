#ifndef HOST_FREERTOS_H
#define HOST_FREERTOS_H
#include <assert.h>
#include <stdint.h>
typedef int32_t BaseType_t;
typedef struct { volatile uint32_t CTRL, CYCCNT; } DWT_Type;
typedef struct { volatile uint32_t DEMCR; } CoreDebug_Type;
typedef struct { volatile uint32_t BSRR, BRR; } GPIO_TypeDef;
extern DWT_Type test_dwt;
extern CoreDebug_Type test_core_debug;
extern GPIO_TypeDef test_gpio;
extern uint32_t test_ipsr, test_primask;
#define DWT (&test_dwt)
#define CoreDebug (&test_core_debug)
#define GPIOA (&test_gpio)
#define CoreDebug_DEMCR_TRCENA_Msk (1u << 24u)
#define DWT_CTRL_NOCYCCNT_Msk (1u << 25u)
#define DWT_CTRL_CYCCNTENA_Msk 1u
#define configASSERT(x) assert(x)
static inline uint32_t __get_IPSR(void) { return test_ipsr; }
static inline uint32_t __get_PRIMASK(void) { return test_primask; }
static inline void __disable_irq(void) { test_primask = 1u; }
static inline void __set_PRIMASK(uint32_t value) { test_primask = value; }
static inline void __DMB(void) { __asm__ volatile("" ::: "memory"); }
static inline void __DSB(void) { __asm__ volatile("" ::: "memory"); }
static inline void __ISB(void) { __asm__ volatile("" ::: "memory"); }
#endif
