#ifndef PLATFORM_UART_H
#define PLATFORM_UART_H

#include <stdbool.h>
#include <stdint.h>

bool uart1_init(uint32_t baud);

void uart1_write_char(char value);
void uart1_write_string(const char *text);
void uart1_write_u32(uint32_t value);
void uart1_flush(void);

#endif