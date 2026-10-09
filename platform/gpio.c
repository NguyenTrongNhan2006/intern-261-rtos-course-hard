#include "gpio.h"

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
