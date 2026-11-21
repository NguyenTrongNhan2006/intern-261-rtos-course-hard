#include "FreeRTOS.h"
#include "task.h"
#include "w4e3_internal.h"

#define TIMER_HZ 1000000u
#define CONTROL_PERIOD_US (TIMER_HZ / W4E1_CONTROL_LOOP_HZ)
#define CONTROL_PHASE_US (CONTROL_PERIOD_US / 2u)

static bool release_valid;
static uint32_t last_release_rt;

void w4e3_control_clock_start(void)
{
    TIM3->SR = 0u;
    TIM3->EGR = TIM_EGR_UG;
    while ((TIM3->SR & TIM_SR_UIF) == 0u) {
        __NOP();
    }
    const uint32_t cycles_per_us = SystemCoreClock / TIMER_HZ;
    const uint32_t left = SysTick->VAL;
    const uint32_t phase = left == 0u ? 0u :
        (SystemCoreClock / configTICK_RATE_HZ - left) / cycles_per_us;

    TIM3->CNT = phase;
    release_valid = false;
    TIM3->SR = 0u;
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    TIM3->CR1 = TIM_CR1_CEN;
}

void w4e3_control_clock_stop(void)
{
    TIM3->CR1 = 0u;
    TIM3->SR = 0u;
    NVIC_ClearPendingIRQ(TIM3_IRQn);
}

void vPortSetupTimerInterrupt(void)
{
    configASSERT(SystemCoreClock == 72000000u);
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;
    TIM3->CR1 = 0u;
    TIM3->PSC = SystemCoreClock / TIMER_HZ - 1u;
    TIM3->ARR = CONTROL_PERIOD_US - 1u;
    TIM3->CCR1 = CONTROL_PHASE_US;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->DIER = TIM_DIER_CC1IE;
    NVIC_SetPriority(TIM3_IRQn, 5u);

    SysTick->CTRL = 0u;
    SysTick->LOAD = SystemCoreClock / configTICK_RATE_HZ - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_CLKSOURCE_Msk;
    w4e3_control_clock_start();
    NVIC_EnableIRQ(TIM3_IRQn);
}

void w4e3_tim3(const uint32_t *frame, uint32_t exc_return) __attribute__((used));

/* EXC_RETURN identifies the stack containing the interrupted PC and LR. */
__attribute__((naked)) void TIM3_IRQHandler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b w4e3_tim3\n");
}

void w4e3_tim3(const uint32_t *frame, uint32_t exc_return)
{
    if ((TIM3->SR & TIM_SR_CC1IF) != 0u) {
        TIM3->SR = ~TIM_SR_CC1IF;
        const uint32_t phase = TIM3->CNT;
        const uint32_t late_us = phase >= CONTROL_PHASE_US ?
            phase - CONTROL_PHASE_US : phase + CONTROL_PERIOD_US - CONTROL_PHASE_US;
        const uint32_t clocks_per_us = SystemCoreClock / TIMER_HZ;
        const uint32_t next_release = DWT->CYCCNT - late_us * clocks_per_us;
        /* TIM2 keeps counting in shallow sleep, even when the CPU is gated. */
        const uint32_t release_rt = w3e1_runtime_stats_now() - late_us / W4E3_RUNTIME_TICK_US;
        const uint32_t period_rt = CONTROL_PERIOD_US / W4E3_RUNTIME_TICK_US;
        uint32_t skipped = 0u;
        if (release_valid) {
            const uint32_t periods = (release_rt - last_release_rt + period_rt / 2u) / period_rt;
            if (periods > 1u) {
                skipped = periods - 1u;
            }
        }
        release_valid = true;
        last_release_rt = release_rt;
        w4e3_control_release_from_isr(next_release, release_rt, skipped, frame, exc_return);
    }
}
