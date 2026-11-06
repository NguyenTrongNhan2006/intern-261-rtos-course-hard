#include "runtime_stats.h"

#include "FreeRTOS.h"
#include "stm32f1xx.h"

#define RUNTIME_HZ 100000u
#define COUNTER_PERIOD 65536u

static volatile uint32_t s_epoch;

/* STEP 1: Đặt TIM2 chạy 100 kHz. Khi APB1 có chia tần, clock TIM2 bằng 2 lần clock APB1. */
void w3e1_runtime_stats_init(void)
{
    const uint32_t apb1 = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    const uint32_t timer_div = (apb1 < 4u) ? 1u : (1u << (apb1 - 4u));
    const uint32_t timer_hz = SystemCoreClock / timer_div;
    configASSERT(timer_hz >= RUNTIME_HZ && timer_hz % RUNTIME_HZ == 0u);

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    TIM2->CR1 = 0u;
    TIM2->DIER = 0u;
    TIM2->PSC = (timer_hz / RUNTIME_HZ) - 1u;
    TIM2->ARR = COUNTER_PERIOD - 1u;
    TIM2->EGR = TIM_EGR_UG;
    /* Đợi sự kiện nạp bộ chia xuất hiện trong SR rồi mới xóa cờ. */
    while ((TIM2->SR & TIM_SR_UIF) == 0u) {
    }
    TIM2->SR = 0u;
    s_epoch = 0u;

    /* Ngắt TIM2 có ưu tiên cao hơn ngưỡng BASEPRI của kernel và không gọi FreeRTOS. */
    NVIC_SetPriority(TIM2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY - 1u);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->DIER = TIM_DIER_UIE;
    TIM2->CR1 = TIM_CR1_CEN;
}

/* STEP 2: Cộng 65.536 vào phần đếm mở rộng sau mỗi lần TIM2 tràn. */
void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) != 0u) {
        TIM2->SR = ~TIM_SR_UIF;
        s_epoch += COUNTER_PERIOD;
    }
}

/* STEP 3: Chụp bộ đếm trong lúc che ngắt bằng PRIMASK.
 * Nếu UIF báo tràn đang chờ, cộng phần tràn và đọc lại CNT để lấy đúng phần thấp. */
uint32_t w3e1_runtime_stats_now(void)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t epoch = s_epoch;
    uint32_t low = TIM2->CNT;
    if ((TIM2->SR & TIM_SR_UIF) != 0u) {
        epoch += COUNTER_PERIOD;
        low = TIM2->CNT;
    }
    __set_PRIMASK(primask);
    return epoch + low;
}
