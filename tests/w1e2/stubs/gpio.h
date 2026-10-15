#ifndef HOST_GPIO_H
#define HOST_GPIO_H
#include "FreeRTOS.h"
void gpio_clock_enable(GPIO_TypeDef *port);
void gpio_clear(GPIO_TypeDef *port, uint8_t pin);
void gpio_config_output_pp_2mhz(GPIO_TypeDef *port, uint8_t pin);
#endif
