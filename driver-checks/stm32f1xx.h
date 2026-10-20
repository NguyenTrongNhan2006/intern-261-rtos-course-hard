#ifndef MOCK_STM32_H
#define MOCK_STM32_H
#include <stdint.h>
#include "constants.h"
#define __NVIC_PRIO_BITS 4u
typedef struct { volatile uint32_t CR1, CR2, OAR1, DR, SR1, SR2, CCR, TRISE; } MockI2C;
typedef struct { volatile uint32_t CFGR, APB1ENR, APB2ENR, APB1RSTR; } MockRCC;
typedef struct { volatile uint32_t CRL, BSRR, BRR, IDR; } MockGPIO;
typedef struct { volatile uint32_t MAPR; } MockAFIO;
typedef struct { volatile uint32_t DEMCR; } MockCore;
typedef struct { volatile uint32_t CTRL, CYCCNT; } MockDWT;
extern MockI2C mock_i2c;
extern MockRCC mock_rcc;
extern MockGPIO mock_gpio;
extern MockAFIO mock_afio;
extern MockCore mock_core;
extern MockDWT mock_clock;
extern uint32_t SystemCoreClock;
extern uint32_t mock_primask, mock_ipsr;
extern unsigned mock_critical;
extern unsigned mock_nvic[2], mock_priorities[2];
MockDWT *mock_dwt(void);
#undef I2C1
#define I2C1 (&mock_i2c)
#define RCC (&mock_rcc)
#define GPIOB (&mock_gpio)
#define AFIO (&mock_afio)
#define CoreDebug (&mock_core)
#define DWT (mock_dwt())
#define CoreDebug_DEMCR_TRCENA_Msk (1u << 24)
#define DWT_CTRL_CYCCNTENA_Msk 1u
enum { I2C1_EV_IRQn = 0, I2C1_ER_IRQn = 1 };
static inline void NVIC_DisableIRQ(int irq) { mock_nvic[irq] = 0u; }
static inline void NVIC_EnableIRQ(int irq) { mock_nvic[irq] = 1u; }
static inline void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
static inline void NVIC_SetPriority(int irq, uint32_t priority) { mock_priorities[irq] = priority; }
static inline uint32_t NVIC_GetPriorityGrouping(void) { return 0u; }
static inline uint32_t __get_PRIMASK(void) { return mock_primask; }
static inline uint32_t __get_BASEPRI(void) { return mock_critical != 0u ? 0x50u : 0u; }
static inline uint32_t __get_IPSR(void) { return mock_ipsr; }
static inline void __disable_irq(void) { mock_primask = 1u; }
static inline void __set_PRIMASK(uint32_t mask) { mock_primask = mask; }
static inline void __NOP(void) {}
#endif
