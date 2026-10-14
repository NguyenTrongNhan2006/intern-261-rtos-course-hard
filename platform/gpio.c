#include "gpio.h"

void gpio_clock_enable(GPIO_TypeDef *port)
{
    if (port == GPIOA) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    } else if (port == GPIOB) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    } else if (port == GPIOC) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    } else {
        return;
    }

    /* Đọc lại sau khi bật peripheral clock. */
    (void)RCC->APB2ENR;
}

void gpio_config_output_pp_2mhz(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin > 15u) {
        return;
    }

    volatile uint32_t *config_register;
    uint32_t shift;

    if (pin < 8u) {
        config_register = &port->CRL;
        shift = 4u * pin;
    } else {
        config_register = &port->CRH;
        shift = 4u * (pin - 8u);
    }

    /* CNF = 00: general-purpose push-pull.
     * MODE = 10: output speed 2 MHz.
     */
    *config_register =
        (*config_register & ~(0xFu << shift)) |
        (0x2u << shift);
}

void gpio_set(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin <= 15u) {
        port->BSRR = 1u << pin;
    }
}

void gpio_clear(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin <= 15u) {
        port->BRR = 1u << pin;
    }
}