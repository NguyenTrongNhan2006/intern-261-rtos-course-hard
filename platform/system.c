#include "stm32f1xx.h"
#include <stdint.h>

uint32_t SystemCoreClock = 72000000u;

void SystemInit(void)
{
    uint32_t timeout;

    /* Bật HSE 8 MHz và chờ sẵn sàng. */
    RCC->CR |= (1u << 16);
    timeout = 1000000u;
    while ((RCC->CR & (1u << 17)) == 0u) {
        if (timeout-- == 0u) {
            for (;;) {
                /* Dừng nếu thạch anh HSE không khởi động. */
            }
        }
    }

    /*
     * Flash: bật prefetch và đặt 2 wait state
     * trước khi chuyển CPU lên 72 MHz.
     */
    FLASH->ACR = (FLASH->ACR & ~0x7u) | (2u << 0) | (1u << 4);

    /*
     * AHB = SYSCLK / 1
     * APB1 = HCLK / 2 = 36 MHz
     * APB2 = HCLK / 1 = 72 MHz
     * ADC = PCLK2 / 6 = 12 MHz
     * PLL = HSE × 9 = 72 MHz
     */
    RCC->CFGR &= ~((0xFu << 4)  |
                   (0x7u << 8)  |
                   (0x7u << 11) |
                   (0x3u << 14) |
                   (1u << 16)   |
                   (1u << 17)   |
                   (0xFu << 18));

    RCC->CFGR |= (0x4u << 8)  |  /* APB1 chia 2 */
                 (0x2u << 14) |  /* ADC chia 6 */
                 (1u << 16)   |  /* PLL lấy nguồn từ HSE */
                 (0x7u << 18);   /* PLL nhân 9 */

    /* Bật PLL và chờ PLL sẵn sàng. */
    RCC->CR |= (1u << 24);
    timeout = 1000000u;
    while ((RCC->CR & (1u << 25)) == 0u) {
        if (timeout-- == 0u) {
            for (;;) {
                /* Dừng nếu PLL không khởi động. */
            }
        }
    }

    /* Chuyển SYSCLK sang PLL và chờ xác nhận. */
    RCC->CFGR = (RCC->CFGR & ~0x3u) | 0x2u;
    timeout = 1000000u;
    while ((RCC->CFGR & (0x3u << 2)) != (0x2u << 2)) {
        if (timeout-- == 0u) {
            for (;;) {
                /* Dừng nếu chưa chuyển được SYSCLK sang PLL. */
            }
        }
    }

    SystemCoreClock = 72000000u;
}
