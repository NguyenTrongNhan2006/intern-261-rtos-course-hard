# W3.1 – UART DMA + IDLE line, 0 mất byte

## Mục tiêu
USART1 RX qua DMA circular, phát hiện biên khung bằng ngắt IDLE, đẩy dữ liệu vào stream buffer, parser lệnh
chạy trong task. 0 byte mất, 0 lỗi CRC trong 10 phút ở baud được giao (`PARAMS.md`).

## Đọc trước
- RM0008 chương USART (bit IDLE, DMA request) và chương DMA (circular mode, half/full transfer interrupt).
- Mastering FreeRTOS – chương Stream Buffers (API, cơ chế trigger-level, dùng từ ISR).

## Việc phải làm
1. Cấu hình USART1 RX nối DMA circular vào 1 buffer tĩnh (không `malloc`), bật ngắt IDLE (USART_CR1.IDLEIE) và
   ngắt half/full transfer của kênh DMA tương ứng.
2. Trong ISR (IDLE + half/full transfer): tính số byte mới nhận (so điểm ghi DMA hiện tại với lần trước, xử lý
   wrap vòng tròn), đẩy đúng số byte đó vào `xStreamBufferSendFromISR`.
3. 1 task gọi `xStreamBufferReceive` để lấy dữ liệu thô, parser lệnh tách khung theo `W3E1_PAYLOAD_LEN_BYTES`
   (xem `params.h`), kiểm CRC32 (zlib CRC32, khớp định dạng `tools/uart_stress.py`), đếm số khung đúng/sai/mất.
4. Chạy `tools/uart_stress.py --port <dev> --baud W3E1_BAUD_BPS --payload-size W3E1_PAYLOAD_LEN_BYTES --rate-hz
   W3E1_FRAME_RATE_HZ --duration-sec 600` (đọc kỹ `--help`, đừng sửa script) liên tục 10 phút.
5. Đo CPU load bằng run-time stats (`vTaskGetRunTimeStats`/`configGENERATE_RUN_TIME_STATS`, cần timer riêng cho
   run-time counter – không dùng SysTick của FreeRTOS).
6. Giải thích trong REPORT.md vì sao chọn kích thước stream buffer / DMA buffer như vậy (tối thiểu để không
   tràn ở tốc độ gửi cao nhất, nhưng không lãng phí RAM 20 KB).

## Sản phẩm nộp
- Driver UART DMA + IDLE + stream buffer + parser lệnh.
- Log 10 phút chạy `tools/uart_stress.py`: 0 lỗi CRC, 0 khung mất.
- Bảng CPU load (run-time stats) trong lúc stress test.

## Tiêu chí đạt
0 byte mất ở baud được giao trong 10 phút liên tục; CPU load đo được và giải thích hợp lý.

## Giờ dự kiến
5 giờ.
