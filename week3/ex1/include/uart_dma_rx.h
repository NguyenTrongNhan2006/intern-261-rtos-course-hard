#ifndef W3E1_UART_DMA_RX_H
#define W3E1_UART_DMA_RX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t frames_ok;
    uint32_t frames_crc_error;
    uint32_t frames_lost;
    uint32_t bytes_received;
    uint32_t frames_length_error;
    uint32_t frames_old_seq;
    uint32_t last_seq;             /* Hợp lệ khi frames_ok khác 0. */
    uint32_t seq_restarts;         /* Số lần host bắt đầu lại từ SEQ 0. */
    uint32_t stream_dropped_bytes;
    uint32_t stream_peak_bytes;
    uint32_t dma_late_events;
    uint32_t dma_errors;
    uint32_t usart_overruns;
    uint32_t usart_framing_errors;
    uint32_t usart_noise_errors;
    uint32_t usart_clear_busy;    /* Vẫn còn byte chờ trước khi xóa cờ. */
} uart_dma_rx_stats_t;

/* STEP 1: Parser Task gọi một lần sau khi scheduler đã chạy. */
void uart_dma_rx_init(void);

/* STEP 2: Thu gom byte mới từ trình phục vụ ngắt USART1 hoặc DMA. */
void uart_dma_rx_on_usart_irq(void);
void uart_dma_rx_on_dma_irq(void);

/* STEP 3: Chờ và phân tích một lô byte. Trả true khi đã kiểm tra ít nhất một khung đủ độ dài. */
bool uart_dma_rx_parse_once(void);

/* STEP 4: Chụp bộ đếm nhất quán từ ngữ cảnh task. */
void uart_dma_rx_get_stats(uart_dma_rx_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif
