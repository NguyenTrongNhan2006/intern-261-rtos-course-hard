#include "uart.h"
#include "stm32f1xx.h"

static uint32_t uart1_pclk2_hz(void)
{
    const uint32_t prescaler =
        (RCC->CFGR & RCC_CFGR_PPRE2) >> 11u;

    const uint32_t divisor =
        (prescaler < 4u) ? 1u : (1u << (prescaler - 3u));

    return SystemCoreClock / divisor;
}

bool uart1_init(uint32_t baud)
{
    const uint32_t pclk2 = uart1_pclk2_hz();

    if ((baud == 0u) || (baud > (pclk2 / 16u))) {
        return false;
    }

    const uint32_t brr = (pclk2 + (baud / 2u)) / baud;

    if ((brr < 16u) || (brr > 0xFFFFu)) {
        return false;
    }

    RCC->APB2ENR |=
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_USART1EN;

    (void)RCC->APB2ENR;

    RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;

    /* PA9: alternate-function push-pull, output speed 2 MHz. */
    GPIOA->CRH =
        (GPIOA->CRH & ~(0xFu << 4u)) |
        (0xAu << 4u);

    /* Asynchronous UART: 8 data bits, no parity, 1 stop bit. */
    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = 0u;

    USART1->BRR = brr;
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;

    return true;
}

void uart1_write_char(char value)
{
    while ((USART1->SR & USART_SR_TXE) == 0u) {
    }

    USART1->DR = (uint32_t)(uint8_t)value;
}

void uart1_write_string(const char *text)
{
    while (*text != '\0') {
        uart1_write_char(*text);
        ++text;
    }
}

void uart1_write_u32(uint32_t value)
{
    char digits[10];
    uint32_t count = 0u;

    do {
        digits[count] = (char)('0' + (value % 10u));
        ++count;
        value /= 10u;
    } while (value != 0u);

    while (count != 0u) {
        --count;
        uart1_write_char(digits[count]);
    }
}

void uart1_write_u64(uint64_t value)
{
    char digits[20];
    uint32_t count = 0u;

    do {
        digits[count] = (char)('0' + (value % 10u));
        ++count;
        value /= 10u;
    } while (value != 0u);

    while (count != 0u) {
        --count;
        uart1_write_char(digits[count]);
    }
}

void uart1_flush(void)
{
    while ((USART1->SR & USART_SR_TC) == 0u) {
    }
}