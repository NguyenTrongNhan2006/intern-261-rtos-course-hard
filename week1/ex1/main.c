#include "FreeRTOS.h"
#include "task.h"
#include "blinky.h"
#include "gpio.h"
#include "params.h"

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
    GPIOA->CRL = (GPIOA->CRL & ~0xFu) | 0xAu;

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
    TIM2->CCMR1 = (6u << 4) | (1u << 3);

    TIM2->CCER = (1u << 0);     /* CC1E; active HIGH */
    TIM2->DIER = 0u;            /* Không dùng interrupt/DMA */

    /* Nạp PSC, ARR và CCR1 vào các thanh ghi hoạt động. */
    TIM2->EGR = (1u << 0);      /* UG */
    TIM2->SR = 0u;             /* Xóa cờ phát sinh khi UG */

    TIM2->CR1 |= (1u << 0);     /* CEN: bắt đầu đếm */
}

static void tag_gpio_init(void)
{
    /* Bật clock GPIOB và AFIO. */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

    /* Tắt JTAG, giữ SWD; các remap khác để mặc định. */
    AFIO->MAPR = (2u << 24);

    /* Chuẩn bị output latch ở mức LOW. */
    gpio_clear(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);

    /* Sau đó mới bật output driver. */
    gpio_config_output_pp_2mhz(
        W1E1_TAG_GPIO_PORT,
        W1E1_TAG_GPIO_PIN
    );
}

static void nvic_priority_init(void)
{
    /* STM32F103 có 4 bits interrupt priority. */
    configASSERT(configPRIO_BITS == 4u);

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