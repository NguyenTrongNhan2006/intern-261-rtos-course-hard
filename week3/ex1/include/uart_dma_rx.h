/* Interface for the W3.1 USART1 RX DMA (circular) + IDLE-line framing +
 * stream-buffer-backed command parser. Baud / payload length / frame rate
 * come from params.h (W3E1_BAUD_BPS, W3E1_PAYLOAD_LEN_BYTES,
 * W3E1_FRAME_RATE_HZ). DMA/USART register setup, the IDLE + half/full
 * transfer ISR bookkeeping (wrap-safe byte-count delta) and the CRC32
 * frame parser are entirely yours to implement. Wire format matches
 * tools/uart_stress.py: 0xAA SYNC, SEQ(u32), LEN(u16), PAYLOAD(LEN),
 * CRC32(u32, zlib.crc32 of PAYLOAD), all little-endian. */
#ifndef W3E1_UART_DMA_RX_H
#define W3E1_UART_DMA_RX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t frames_ok;        /* CRC matched, length sane */
  uint32_t frames_crc_error; /* CRC mismatch */
  uint32_t frames_lost;      /* gap detected in SEQ */
  uint32_t bytes_received;   /* raw byte count, from DMA NDTR delta */
} uart_dma_rx_stats_t;

/* Call once before vTaskStartScheduler(): configures USART1 RX AF input,
 * baud = W3E1_BAUD_BPS, DMA channel in circular mode into the internal
 * static ring buffer, enables IDLE + DMA half/full-transfer interrupts and
 * NVIC, and creates the internal static stream buffer. */
void uart_dma_rx_init(void);

/* Call from USART1_IRQHandler() (IDLE flag) and the DMA channel's
 * IRQHandler (half/full transfer complete): computes how many new raw
 * bytes arrived since the last call (handling circular wrap) and pushes
 * them into the stream buffer via xStreamBufferSendFromISR(). */
void uart_dma_rx_on_usart_irq(void);
void uart_dma_rx_on_dma_irq(void);

/* Task-context parser: blocks on xStreamBufferReceive() for raw bytes,
 * reassembles 0xAA-framed packets (SYNC, SEQ, LEN, PAYLOAD, CRC32),
 * validates CRC32 and SEQ continuity, and updates the internal stats.
 * Call in a loop from the consumer task. Returns true if a complete frame
 * was processed this call (ok or not), false if it merely buffered
 * partial bytes. */
bool uart_dma_rx_parse_once(void);

/* Snapshot of the running counters (ok / crc-error / lost / raw bytes). */
void uart_dma_rx_get_stats(uart_dma_rx_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* W3E1_UART_DMA_RX_H */
