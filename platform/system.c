#include "stm32f1xx.h"
#include <stdint.h>

/* Sau reset, STM32F103 sử dụng HSI 8 MHz. */
uint32_t SystemCoreClock = 8000000u;

void SystemInit(void)
{
    uint32_t timeout;

    /* 1. Bật HSE bật Bit 16 lên 1 : board sử dụng thạch anh ngoài 8 MHz. */

    // Clock cControl register (RCC_CR) : 7.3.3
    RCC->CR |= RCC_CR_HSEON; 

    timeout = 1000000u;

    while ((RCC->CR & RCC_CR_HSERDY) == 0u) {
        if (timeout-- == 0u) {
            for (;;) {
                /* HSE không khởi động được. */
            }
        }
    }

    /*
     * 2. Cấu hình Flash trước khi tăng clock:
     *    - Prefetch enabled.
     *    - 2 wait-state cho HCLK 72 MHz.
     * Chu Kì chờ FLASH_ACR_LATENCY_0 có giá trị 0x0:
     * FLASH_ACR_LATENCY_1 có giá trị 0x2:
     * đây là bit mask tạo giá trị LATENCY = 2.
     * HardFault / BusFault ???
     */

    // FLASH->ACR (Flash Access Control Register :3.3.3

      FLASH->ACR =
    (FLASH->ACR & ~FLASH_ACR_LATENCY) |
    FLASH_ACR_LATENCY_1 |
    FLASH_ACR_PRFTBE;// (Prefetch Buffer)

    /*
     * 3. Cấu hình clock tree.
     * Hàm này được gọi từ Reset_Handler sau reset phần cứng,
     * khi PLL vẫn đang tắt.
     *
     * HSE          = 8 MHz
     * PLL          = HSE x 9 = 72 MHz
     * AHB / HCLK   = SYSCLK / 1 = 72 MHz
     * APB1 / PCLK1 = HCLK / 2 = 36 MHz
     * APB2 / PCLK2 = HCLK / 1 = 72 MHz
     * ADC clock    = PCLK2 / 6 = 12 MHz
     */

     // Thanh ghi RCC->CFGR (Clock Configuration Register : 7.3.2)

    RCC->CFGR &=
        ~(RCC_CFGR_HPRE |
          RCC_CFGR_PPRE1 |
          RCC_CFGR_PPRE2 |
          RCC_CFGR_ADCPRE |
          RCC_CFGR_PLLSRC |
          RCC_CFGR_PLLXTPRE |
          RCC_CFGR_PLLMULL);

    RCC->CFGR |=
        RCC_CFGR_PPRE1_DIV2 |
        RCC_CFGR_ADCPRE_DIV6 |
        RCC_CFGR_PLLSRC |
        RCC_CFGR_PLLMULL9;

    /* 4. Bật PLL và chờ PLL ổn định. */
    RCC->CR |= RCC_CR_PLLON;

    timeout = 1000000u;

    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) {
        if (timeout-- == 0u) {
            for (;;) {
                /* PLL không khóa được. */
            }
        }
    }

    /* 5. Chọn PLL làm nguồn SYSCLK. */
    RCC->CFGR =
        (RCC->CFGR & ~RCC_CFGR_SW) |
        RCC_CFGR_SW_PLL;

    timeout = 1000000u;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
        if (timeout-- == 0u) {
            for (;;) {
                /* Không chuyển được SYSCLK sang PLL. */
            }
        }
    }

    /* Hoàn tất các thay đổi cấu hình trước khi chạy tiếp. */ 

    // __DSB() (Data Synchronization Barrier) & __ISB() (Instruction Synchronization Barrier):

    __DSB();
    // buộc CPU chờ hoàn tất hẳn trên bus.
    __ISB(); 
    // xóa (flush) và nạp lại  (72 MHz, 2 wait-states).

    /* Chỉ cập nhật sau khi xác nhận chuyển clock thành công. */
    SystemCoreClock = 72000000u;
}