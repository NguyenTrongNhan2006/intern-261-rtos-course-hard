#include <stdint.h>

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern void SystemInit(void);
extern int main(void);

void Default_Handler(void)
{
    for (;;) {
        /* Dừng tại đây nếu có exception chưa được xử lý. */
    }
}

#define WEAK_DEFAULT_HANDLER(name) \
    void name(void) __attribute__((weak, alias("Default_Handler")))

WEAK_DEFAULT_HANDLER(NMI_Handler);
WEAK_DEFAULT_HANDLER(HardFault_Handler);
WEAK_DEFAULT_HANDLER(MemManage_Handler);
WEAK_DEFAULT_HANDLER(BusFault_Handler);
WEAK_DEFAULT_HANDLER(UsageFault_Handler);
WEAK_DEFAULT_HANDLER(SVC_Handler);
WEAK_DEFAULT_HANDLER(DebugMon_Handler);
WEAK_DEFAULT_HANDLER(PendSV_Handler);
WEAK_DEFAULT_HANDLER(SysTick_Handler);
WEAK_DEFAULT_HANDLER(I2C1_EV_IRQHandler);
WEAK_DEFAULT_HANDLER(I2C1_ER_IRQHandler);
/* W2.2: Handler mặc định nếu ứng dụng chưa cung cấp ISR TIM2. */
WEAK_DEFAULT_HANDLER(TIM2_IRQHandler);

typedef void (*isr_handler_t)(void);

extern uint32_t _estack;

void Reset_Handler(void);

__attribute__((section(".isr_vector"), used))
const isr_handler_t vector_table[] = {
    (isr_handler_t)&_estack, /* Initial stack pointer */
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,               /* Reserved */
    SVC_Handler,
    DebugMon_Handler,
    0,                        /* Reserved */
    PendSV_Handler,
    SysTick_Handler,

    /* IRQ 0–5: WWDG, PVD, TAMPER, RTC, FLASH, RCC */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler,

    /* IRQ 6–10: EXTI0–EXTI4 */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler,

    /* IRQ 11–17: DMA1 Channel 1–7 */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler,

    /* IRQ 18–22: ADC1_2, USB/CAN, CAN, CAN */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler,

    /* IRQ 23: EXTI9_5 */
    Default_Handler,

    /* IRQ 24–27: TIM1 */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler,

      /* W2.2: Nối IRQ28 với ISR producer trong ring_app.c. */
TIM2_IRQHandler,   /* IRQ28: TIM2. */
Default_Handler,  /* IRQ29: TIM3. */
Default_Handler,  /* IRQ30: TIM4. */

    /* IRQ 31–34: I2C1 và I2C2 */
    I2C1_EV_IRQHandler, // IRQ 31: sự kiện I2C1.
    I2C1_ER_IRQHandler, // IRQ 32: lỗi I2C1.
    Default_Handler,   // IRQ 33: I2C2_EV.
    Default_Handler,   // IRQ 34: I2C2_ER.

    /* IRQ 35–39: SPI1, SPI2, USART1, USART2, USART3 */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler,

    /* IRQ 40–42: EXTI15_10, RTCAlarm, USBWakeUp */
    Default_Handler, Default_Handler, Default_Handler
};

void Reset_Handler(void)
{
    const volatile uint32_t *source = &_sidata;
    volatile uint32_t *destination = &_sdata;

    while (destination < &_edata) {
        *destination++ = *source++;
    }

    for (destination = &_sbss;
         destination < &_ebss;
         destination++) {
        *destination = 0u;
    }

    SystemInit();
    (void)main();

    for (;;) {
        /* main không nên trả về. */
    }
// NO RETURN  !!!
}