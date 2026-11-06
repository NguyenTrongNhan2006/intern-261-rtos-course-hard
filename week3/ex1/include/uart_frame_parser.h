#ifndef W3E1_UART_FRAME_PARSER_H
#define W3E1_UART_FRAME_PARSER_H

#include "uart_dma_rx.h"

/* STEP 1: Chỉ Parser Task được đưa byte vào máy trạng thái. */
bool uart_frame_parser_feed(uint8_t byte);
/* STEP 2: Sao chép bộ đếm parser. Bên gọi giữ vùng găng khi lấy mẫu từ task khác. */
void uart_frame_parser_get_stats(uart_dma_rx_stats_t *out);

#endif
