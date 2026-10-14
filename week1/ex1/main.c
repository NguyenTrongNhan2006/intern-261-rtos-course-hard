#include "FreeRTOS.h"
#include "task.h"
#include "blinky.h"
#include "gpio.h"
#include "params.h"

_Static_assert(
    configPRIO_BITS == 4u,
    "STM32F103 requires 4 interrupt priority bits"
);

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();

    for (;;) {
        /* Dừng tại đây để debug khi cấp phát heap thất bại. */
    }
}

static void clock_probe_init(void)
{
    /* Bật clock GPIOA và TIM2. */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* Reset TIM2 về trạng thái xác định trước khi cấu hình. */
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;

    /*
     * PA0: Alternate function push-pull, output speed 2 MHz.
     * CNF0 = 10, MODE0 = 10 → nibble 0xA.
     * TIM2_CH1 dùng PA0 khi không remap.
     */
    GPIOA->CRL =
    (GPIOA->CRL & ~0xFu) |
    GPIO_CRL_MODE0_1 |
    GPIO_CRL_CNF0_1;

TIM2->CR1 = TIM_CR1_ARPE;

    /* Up-counting, bật auto-reload preload. */
    TIM2->CR1 = (1u << 7);       /* ARPE */

    TIM2->PSC = 71u;
    TIM2->ARR = 999u;
    TIM2->CCR1 = 500u;

    /*
     * CC1S = 00: channel 1 là output.
     * OC1M = 110: PWM mode 1.
     * OC1PE = 1: bật CCR1 preload.
     */
    TIM2->CCMR1 =
    TIM_CCMR1_OC1M_1 |
    TIM_CCMR1_OC1M_2 |
    TIM_CCMR1_OC1PE;

TIM2->CCER = TIM_CCER_CC1E;
TIM2->DIER = 0u;

TIM2->EGR = TIM_EGR_UG;
TIM2->SR = 0u;

TIM2->CR1 |= TIM_CR1_CEN;
}

static void tag_gpio_init(void)
{
    gpio_clock_enable(W1E1_TAG_GPIO_PORT);

    const uint32_t needs_jtag_release =
        ((W1E1_TAG_GPIO_PORT == GPIOA) &&
         (W1E1_TAG_GPIO_PIN == 15u)) ||
        ((W1E1_TAG_GPIO_PORT == GPIOB) &&
         ((W1E1_TAG_GPIO_PIN == 3u) ||
          (W1E1_TAG_GPIO_PIN == 4u)));

    if (needs_jtag_release != 0u) {
        RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
        (void)RCC->APB2ENR;

        /*
         * Giữ các remap khác.
         * Ghi lại toàn bộ SWJ_CFG: tắt JTAG, giữ SWD.
         */
        AFIO->MAPR =
            (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) |
            AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
    }

    /* Đặt latch LOW trước khi bật output driver. */
    gpio_clear(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);

    gpio_config_output_pp_2mhz(
        W1E1_TAG_GPIO_PORT,
        W1E1_TAG_GPIO_PIN
    );
}

static void nvic_priority_init(void)
{
    /* VECTKEY = 0x5FA; PRIGROUP = 3:
     * 4 bits preemption priority, 0 bits subpriority.
     */
    SCB->AIRCR =
        (0x5FAu << SCB_AIRCR_VECTKEY_Pos) |
        (3u << SCB_AIRCR_PRIGROUP_Pos);

    __DSB();
    __ISB();
}

int main(void)
{
    nvic_priority_init();
    tag_gpio_init();
    clock_probe_init();        /* Thêm dòng này */

    blinky_app_init();
    vTaskStartScheduler();

    configASSERT(0);

    for (;;) {
    }
}