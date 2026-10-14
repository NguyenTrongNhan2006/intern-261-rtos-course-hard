#ifndef PLATFORM_GPIO_H
#define PLATFORM_GPIO_H

#include <stdint.h>
#include "stm32f1xx.h"

void gpio_config_output_pp_2mhz(GPIO_TypeDef *port, uint8_t pin);
void gpio_set(GPIO_TypeDef *port, uint8_t pin);
void gpio_clear(GPIO_TypeDef *port, uint8_t pin);
void gpio_clock_enable(GPIO_TypeDef *port);

#endif