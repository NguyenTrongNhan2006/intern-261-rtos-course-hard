#include "uart_dma_rx.h"

#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "task.h"

#include "params.h"
#include "stm32f1xx.h"
#include "uart.h"
#include "uart_frame_parser.h"

enum {
    DMA_BYTES = 256,
    STREAM_BYTES = 512,
    PARSER_BATCH_BYTES = 128
};

#define USART_RX_ERRORS (USART_SR_ORE | USART_SR_FE | USART_SR_NE)
#define DMA_BOUNDARIES (DMA_ISR_HTIF5 | DMA_ISR_TCIF5)

/* STEP 1: Cấp phát tĩnh vòng DMA, Stream Buffer và vùng đọc theo lô.
 * Mảng Stream Buffer 512 byte chứa tối đa 511 byte vì kernel chừa một ô.
 * Phép & (DMA_BYTES - 1) chỉ đúng khi DMA_BYTES là lũy thừa của 2. */
_Static_assert((DMA_BYTES & (DMA_BYTES - 1)) == 0, "DMA_BYTES phai la luy thua cua 2");

static uint8_t s_dma_ring[DMA_BYTES];
static uint8_t s_stream_storage[STREAM_BYTES];
static uint8_t s_parser_batch[PARSER_BATCH_BYTES];
static StaticStreamBuffer_t s_stream_control;
static StreamBufferHandle_t s_stream;
static size_t s_last_pos;
static volatile uart_dma_rx_stats_t s_rx;

/* STEP 2: Cấu hình USART1 và bật DMA vòng trước khi cho phép nhận byte. */
void uart_dma_rx_init(void)
{
    s_stream = xStreamBufferCreateStatic(sizeof(s_stream_storage), 1u,
                                        s_stream_storage, &s_stream_control);
    configASSERT(s_stream != NULL);

    const bool uart_ready = uart1_init(W3E1_BAUD_BPS);
    configASSERT(uart_ready);

    /* PA10 là ngõ vào kéo lên (CNF=10, ODR10=1) để chân RX không nhận nhiễu khi rút dây.
     * uart1_init đã cấu hình PA9 và bộ chia baud. */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFu << 8u)) | (0x8u << 8u);
    GPIOA->BSRR = GPIO_BSRR_BS10;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    (void)RCC->AHBENR;

    DMA1_Channel5->CCR = 0u;
    DMA1->IFCR = DMA_IFCR_CHTIF5 | DMA_IFCR_CTCIF5 | DMA_IFCR_CTEIF5;
    DMA1_Channel5->CPAR = (uint32_t)(uintptr_t)&USART1->DR;
    DMA1_Channel5->CMAR = (uint32_t)(uintptr_t)s_dma_ring;
    DMA1_Channel5->CNDTR = DMA_BYTES;
    DMA1_Channel5->CCR = DMA_CCR_PL_1 | DMA_CCR_MINC | DMA_CCR_CIRC |
                        DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_TEIE | DMA_CCR_EN;

    const uint32_t priority = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY;
    NVIC_SetPriority(USART1_IRQn, priority);
    NVIC_SetPriority(DMA1_Channel5_IRQn, priority);
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel5_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);

    USART1->CR3 = USART_CR3_DMAR | USART_CR3_EIE;
    USART1->CR1 |= USART_CR1_IDLEIE | USART_CR1_RE;
}

/* STEP 3: Chốt vị trí ghi và chép byte mới trong vùng găng của ngắt. */
static void send_span(size_t pos, size_t length, BaseType_t *woken)
{
    const size_t sent = xStreamBufferSendFromISR(s_stream, &s_dma_ring[pos], length, woken);
    s_rx.stream_dropped_bytes += (uint32_t)(length - sent);
}

static void collect_locked(BaseType_t *woken, bool cursor_uncertain)
{
    const size_t remaining = DMA1_Channel5->CNDTR;
    const size_t write_pos = (DMA_BYTES - remaining) & (DMA_BYTES - 1u);
    __DMB();

    /* Hai cờ HT và TC cùng chờ có thể che mất cả một vòng DMA.
     * Bỏ đoạn không xác định, chốt lại vị trí đọc và ghi lỗi dma_late. */
    if (cursor_uncertain) {
        ++s_rx.dma_late_events;
        s_last_pos = write_pos;
        return;
    }

    const size_t count = (write_pos - s_last_pos) & (DMA_BYTES - 1u);
    if (count == 0u) {
        return;
    }

    const size_t tail = DMA_BYTES - s_last_pos;
    const size_t first = (count < tail) ? count : tail;
    send_span(s_last_pos, first, woken);
    if (first < count) {
        send_span(0u, count - first, woken);
    }
    s_last_pos = write_pos;
    s_rx.bytes_received += (uint32_t)count;

    const size_t queued = xStreamBufferBytesAvailable(s_stream);
    if (queued > s_rx.stream_peak_bytes) {
        s_rx.stream_peak_bytes = (uint32_t)queued;
    }
}

/* STEP 4: Xóa cờ USART và DMA tại nguồn, rồi thu gom byte mới. */
void uart_dma_rx_on_usart_irq(void)
{
    BaseType_t woken = pdFALSE;
    const UBaseType_t mask = portSET_INTERRUPT_MASK_FROM_ISR();
    const uint32_t status = USART1->SR;
    if ((status & (USART_SR_IDLE | USART_RX_ERRORS)) != 0u) {
        /* Xóa IDLE và các cờ lỗi bằng cách đọc SR rồi DR. Nếu RXNE đang bật,
         * chờ có giới hạn để DMA lấy byte trước, tránh kẹt ngắt khi DMA lỗi. */
        uint32_t before_read = status;
        for (unsigned i = 0u; i < 32u && (before_read & USART_SR_RXNE) != 0u; ++i) {
            before_read = USART1->SR;
        }
        (void)USART1->DR;
        s_rx.usart_overruns += ((status | before_read) & USART_SR_ORE) != 0u;
        s_rx.usart_framing_errors += ((status | before_read) & USART_SR_FE) != 0u;
        s_rx.usart_noise_errors += ((status | before_read) & USART_SR_NE) != 0u;
        s_rx.usart_clear_busy += (before_read & USART_SR_RXNE) != 0u;

        /* Giao trường hợp cả HT và TC đang chờ cho ngắt DMA xử lý một lần. */
        const uint32_t dma_status = DMA1->ISR;
        if ((dma_status & DMA_BOUNDARIES) != DMA_BOUNDARIES &&
            (dma_status & DMA_ISR_TEIF5) == 0u) {
            collect_locked(&woken, false);
        }
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    portYIELD_FROM_ISR(woken);
}

void uart_dma_rx_on_dma_irq(void)
{
    BaseType_t woken = pdFALSE;
    const UBaseType_t mask = portSET_INTERRUPT_MASK_FROM_ISR();
    const uint32_t status = DMA1->ISR;
    uint32_t clear = 0u;

    if ((status & DMA_ISR_HTIF5) != 0u) {
        clear |= DMA_IFCR_CHTIF5;
    }
    if ((status & DMA_ISR_TCIF5) != 0u) {
        clear |= DMA_IFCR_CTCIF5;
    }
    if ((status & DMA_ISR_TEIF5) != 0u) {
        clear |= DMA_IFCR_CTEIF5;
        ++s_rx.dma_errors;
        /* Lỗi truyền DMA làm phiên đo không hợp lệ. Dừng nhận và giữ TX để báo lỗi. */
        USART1->CR1 &= ~(USART_CR1_IDLEIE | USART_CR1_RE);
        USART1->CR3 &= ~(USART_CR3_DMAR | USART_CR3_EIE);
        DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    }

    if (clear != 0u) {
        DMA1->IFCR = clear;
        if ((status & DMA_ISR_TEIF5) == 0u) {
            collect_locked(&woken, (status & DMA_BOUNDARIES) == DMA_BOUNDARIES);
        }
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    portYIELD_FROM_ISR(woken);
}

void USART1_IRQHandler(void)
{
    uart_dma_rx_on_usart_irq();
}

void DMA1_Channel5_IRQHandler(void)
{
    uart_dma_rx_on_dma_irq();
}

/* STEP 5: Đọc theo lô trong Parser Task và chụp bộ đếm trong vùng găng.
 * Số byte đọc mỗi lần có thể cắt ngang khung hoặc chứa nhiều khung. */
bool uart_dma_rx_parse_once(void)
{
    const size_t count = xStreamBufferReceive(s_stream, s_parser_batch,
                                              sizeof(s_parser_batch), portMAX_DELAY);
    bool completed = false;
    for (size_t i = 0u; i < count; ++i) {
        if (uart_frame_parser_feed(s_parser_batch[i])) {
            completed = true;
        }
    }
    return completed;
}

void uart_dma_rx_get_stats(uart_dma_rx_stats_t *out)
{
    taskENTER_CRITICAL();
    uart_frame_parser_get_stats(out);
    out->bytes_received = s_rx.bytes_received;
    out->stream_dropped_bytes = s_rx.stream_dropped_bytes;
    out->stream_peak_bytes = s_rx.stream_peak_bytes;
    out->dma_late_events = s_rx.dma_late_events;
    out->dma_errors = s_rx.dma_errors;
    out->usart_overruns = s_rx.usart_overruns;
    out->usart_framing_errors = s_rx.usart_framing_errors;
    out->usart_noise_errors = s_rx.usart_noise_errors;
    out->usart_clear_busy = s_rx.usart_clear_busy;
    taskEXIT_CRITICAL();
}
