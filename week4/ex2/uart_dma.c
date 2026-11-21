#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "task.h"

#include "../ex1/include/mixed_criticality_design.h"
#include "w4e2_internal.h"

#include <string.h>

#ifndef W4E2_UART_BAUD_BPS
#define W4E2_UART_BAUD_BPS W4E1_TELEMETRY_BAUD_BPS
#endif

#define RX_BYTES W4E1_DESIGN_UART_RX_RING_BYTES
#define RX_COOLDOWN_MS 50u
#define RX_WAKE_LISTEN_MS 100u
#define RX_ERRORS (USART_SR_ORE | USART_SR_FE | USART_SR_NE)
#define RX_BOUNDARIES (DMA_ISR_HTIF5 | DMA_ISR_TCIF5)
#define RX_DMA_MODE (DMA_CCR_PL_1 | DMA_CCR_MINC | DMA_CCR_CIRC | \
                     DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_TEIE)

static uint8_t rx_ring[RX_BYTES], rx_storage[512u];
static StaticStreamBuffer_t rx_control;
static StreamBufferHandle_t rx_stream;
static size_t rx_cursor;
static bool rx_followup;
static volatile uint32_t rx_faults;
static TickType_t rx_fault_tick, listen_start;
static volatile bool cooling;
static bool listening;

static uint8_t tx_buffer[2][W4E2_UART_TX_BYTES];
static size_t tx_length[2];
static StaticSemaphore_t tx_control;
static SemaphoreHandle_t tx_mutex;
static TaskHandle_t tx_waiter;
static uint32_t tx_head, tx_count;
static volatile uint32_t tx_errors;

_Static_assert((RX_BYTES & (RX_BYTES - 1u)) == 0u, "RX DMA ring must be a power of two");

static void enable_rx(void)
{
    DMA1_Channel5->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF5;
    DMA1_Channel5->CNDTR = RX_BYTES;
    rx_cursor = 0u;
    rx_followup = false;
    (void)USART1->SR;
    (void)USART1->DR;
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel5_IRQn);
    DMA1_Channel5->CCR = RX_DMA_MODE | DMA_CCR_EN;
    USART1->CR3 |= USART_CR3_DMAR | USART_CR3_EIE;
    USART1->CR1 |= USART_CR1_RE | USART_CR1_IDLEIE;
}

static void disable_rx(void)
{
    USART1->CR1 &= ~(USART_CR1_IDLEIE | USART_CR1_RE);
    USART1->CR3 &= ~(USART_CR3_DMAR | USART_CR3_EIE);
    DMA1_Channel5->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF5;
    (void)USART1->SR;
    (void)USART1->DR;
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel5_IRQn);
}

void w4e2_uart_init(void)
{
    rx_stream = xStreamBufferCreateStatic(sizeof rx_storage, 1u, rx_storage, &rx_control);
    tx_mutex = xSemaphoreCreateMutexStatic(&tx_control);
    configASSERT(rx_stream != NULL && tx_mutex != NULL);
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    (void)RCC->AHBENR;
    GPIOA->BSRR = GPIO_BSRR_BS10;
    GPIOA->CRH = (GPIOA->CRH & ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9 |
                                GPIO_CRH_MODE10 | GPIO_CRH_CNF10)) |
                 GPIO_CRH_MODE9 | GPIO_CRH_CNF9_1 | GPIO_CRH_CNF10_1;
    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = USART_CR3_DMAT;
    USART1->BRR = (SystemCoreClock + W4E2_UART_BAUD_BPS / 2u) / W4E2_UART_BAUD_BPS;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;

    DMA1_Channel4->CCR = 0u;
    DMA1_Channel4->CPAR = (uint32_t)(uintptr_t)&USART1->DR;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    DMA1_Channel5->CCR = 0u;
    DMA1_Channel5->CPAR = (uint32_t)(uintptr_t)&USART1->DR;
    DMA1_Channel5->CMAR = (uint32_t)(uintptr_t)rx_ring;
    enable_rx();

    /* Same priority serializes the two RX producers and the TX queue ISR. */
    NVIC_SetPriority(USART1_IRQn, 7u);
    NVIC_SetPriority(DMA1_Channel5_IRQn, 7u);
    NVIC_SetPriority(DMA1_Channel4_IRQn, 7u);
    NVIC_ClearPendingIRQ(DMA1_Channel4_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    NVIC_EnableIRQ(DMA1_Channel4_IRQn);
}

static size_t rx_position(void)
{
    return (RX_BYTES - DMA1_Channel5->CNDTR) & (RX_BYTES - 1u);
}

static void begin_cooldown(TickType_t now)
{
    disable_rx();
    rx_fault_tick = now;
    cooling = true;
    ++rx_faults;
}

void w4e2_uart_bad_frame(void)
{
    taskENTER_CRITICAL();
    if (!cooling) {
        begin_cooldown(xTaskGetTickCount());
    }
    taskEXIT_CRITICAL();
}

bool w4e2_uart_cooling(void)
{
    taskENTER_CRITICAL();
    if (cooling && xTaskGetTickCount() - rx_fault_tick >= pdMS_TO_TICKS(RX_COOLDOWN_MS)) {
        /* The shell is the only reader and RX is off. No task waits on the stream. */
        const BaseType_t reset = xStreamBufferReset(rx_stream);
        configASSERT(reset == pdPASS);
        cooling = false;
        enable_rx();
    }
    const bool still_cooling = cooling;
    taskEXIT_CRITICAL();
    return still_cooling;
}

bool w4e2_uart_fault_active(void)
{
    return cooling;
}

uint32_t w4e2_uart_rx_faults(void)
{
    return rx_faults;
}

uint32_t w4e2_uart_tx_errors(void)
{
    return tx_errors;
}

static void collect_from_isr(BaseType_t *woken)
{
    const size_t position = rx_position();
    const size_t count = (position - rx_cursor) & (RX_BYTES - 1u);
    if (count == 0u) {
        return;
    }
    /* A late half interrupt can include the first few bytes of the next half.
     * Leave at least a quarter-ring of headroom while the ISR copies. */
    if (count > RX_BYTES - RX_BYTES / 4u) {
        begin_cooldown(xTaskGetTickCountFromISR());
        return;
    }
    const size_t batch = count > RX_BYTES / 2u ? RX_BYTES / 2u : count;
    __DMB();
    const size_t available = RX_BYTES - rx_cursor;
    const size_t first = batch < available ? batch : available;
    size_t sent = xStreamBufferSendFromISR(rx_stream, rx_ring + rx_cursor, first, woken);
    if (sent == first && first < batch) {
        sent += xStreamBufferSendFromISR(rx_stream, rx_ring, batch - first, woken);
    }
    rx_cursor = (rx_cursor + batch) & (RX_BYTES - 1u);
    if (sent != batch) {
        begin_cooldown(xTaskGetTickCountFromISR());
    } else if (batch < count) {
        rx_followup = true;
        NVIC_SetPendingIRQ(DMA1_Channel5_IRQn);
    }
}

void USART1_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    const uint32_t status = USART1->SR;
    if ((status & (USART_SR_IDLE | RX_ERRORS)) != 0u) {
        uint32_t before_dr = status;
        for (uint32_t n = 0u; n < 32u && (before_dr & USART_SR_RXNE) != 0u; ++n) {
            before_dr = USART1->SR;
        }
        (void)USART1->DR;
        if (!cooling) {
            const uint32_t dma = DMA1->ISR;
            if (((status | before_dr) & RX_ERRORS) != 0u ||
                (before_dr & USART_SR_RXNE) != 0u) {
                begin_cooldown(xTaskGetTickCountFromISR());
            } else if ((dma & RX_BOUNDARIES) != RX_BOUNDARIES &&
                       (dma & DMA_ISR_TEIF5) == 0u) {
                collect_from_isr(&woken);
            }
        }
    }
    portYIELD_FROM_ISR(woken);
}

void DMA1_Channel5_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    const uint32_t flags = DMA1->ISR & (RX_BOUNDARIES | DMA_ISR_TEIF5);
    /* ISR and IFCR have the same bit positions. Clear only the captured flags. */
    DMA1->IFCR = flags;
    const bool followup = rx_followup;
    rx_followup = false;
    if (!cooling && (flags != 0u || followup)) {
        if ((flags & DMA_ISR_TEIF5) != 0u || (flags & RX_BOUNDARIES) == RX_BOUNDARIES) {
            begin_cooldown(xTaskGetTickCountFromISR());
        } else {
            collect_from_isr(&woken);
        }
    }
    portYIELD_FROM_ISR(woken);
}

size_t w4e2_uart_receive(uint8_t *byte, TickType_t wait)
{
    return xStreamBufferReceive(rx_stream, byte, 1u, wait);
}

static void start_tx(void)
{
    USART1->SR = ~USART_SR_TC;
    DMA1_Channel4->CMAR = (uint32_t)(uintptr_t)tx_buffer[tx_head];
    DMA1_Channel4->CNDTR = tx_length[tx_head];
    __DMB();
    DMA1_Channel4->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE |
                         DMA_CCR_TEIE | DMA_CCR_EN;
}

void DMA1_Channel4_IRQHandler(void)
{
    const uint32_t flags = DMA1->ISR & (DMA_ISR_TCIF4 | DMA_ISR_TEIF4);
    if (flags == 0u) {
        return;
    }
    DMA1_Channel4->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CGIF4;
    if ((flags & DMA_ISR_TEIF4) != 0u) {
        ++tx_errors;
    }
    if (tx_count != 0u) {
        --tx_count;
        tx_head ^= 1u;
        if (tx_count != 0u) {
            start_tx();
        }
    }
    BaseType_t woken = pdFALSE;
    if (tx_waiter != NULL) {
        vTaskNotifyGiveIndexedFromISR(tx_waiter, W4E2_IO_NOTIFY_INDEX, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

bool w4e2_uart_send(const void *data, size_t len, TickType_t wait)
{
    if (len == 0u || len > W4E2_UART_TX_BYTES) {
        return false;
    }
    const TickType_t start = xTaskGetTickCount();
    if (xSemaphoreTake(tx_mutex, wait) != pdTRUE) {
        return false;
    }

    bool queued = false;
    (void)ulTaskNotifyTakeIndexed(W4E2_IO_NOTIFY_INDEX, pdTRUE, 0u);
    taskENTER_CRITICAL();
    if (tx_count == 2u && wait != 0u) {
        tx_waiter = xTaskGetCurrentTaskHandle();
        taskEXIT_CRITICAL();
        const TickType_t spent = xTaskGetTickCount() - start;
        if (spent < wait) {
            (void)ulTaskNotifyTakeIndexed(W4E2_IO_NOTIFY_INDEX, pdTRUE, wait - spent);
        }
        taskENTER_CRITICAL();
        tx_waiter = NULL;
    }
    if (tx_count < 2u) {
        /* Reserve the free slot. Only the ISR may advance tx_head while
         * interrupts are open and it cannot access the uncommitted slot. */
        const uint32_t slot = (tx_head + tx_count) & 1u;
        taskEXIT_CRITICAL();
        memcpy(tx_buffer[slot], data, len);
        tx_length[slot] = len;
        taskENTER_CRITICAL();
        if (tx_count++ == 0u) {
            start_tx();
        }
        queued = true;
    }
    taskEXIT_CRITICAL();
    (void)xSemaphoreGive(tx_mutex);
    return queued;
}

bool w4e2_uart_prepare_stop(void)
{
    /* Called from the idle task with PRIMASK set. TX DMA TC alone is too early:
     * USART TC also covers the final stop bit on the wire. */
    if (listening && xTaskGetTickCount() - listen_start >= pdMS_TO_TICKS(RX_WAKE_LISTEN_MS)) {
        listening = false;
    }
    if (listening || cooling || tx_count != 0u || (DMA1_Channel4->CCR & DMA_CCR_EN) != 0u ||
        (USART1->SR & (USART_SR_TC | USART_SR_IDLE | USART_SR_RXNE | RX_ERRORS)) != USART_SR_TC ||
        (DMA1->ISR & (RX_BOUNDARIES | DMA_ISR_TEIF5)) != 0u ||
        rx_position() != rx_cursor || !xStreamBufferIsEmpty(rx_stream) ||
        (GPIOA->IDR & GPIO_IDR_IDR10) == 0u) {
        return false;
    }
    disable_rx();
    return true;
}

void w4e2_uart_resume(bool rx_edge)
{
    if (rx_edge) {
        listening = true;
        listen_start = xTaskGetTickCount();
    }
    enable_rx();
}
