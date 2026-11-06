# W3.1 – PREDICTION trước khi chạy stress test

## 1. Thông số và định dạng trên dây

Nguồn thông số là [include/params.h](include/params.h), [PARAMS.md](PARAMS.md) và [tools/uart_stress.py](../../tools/uart_stress.py). Các con số dưới đây dùng cấu hình UART 8N1 (8 bit dữ liệu, không parity, 1 bit stop), cộng 1 bit start cho mỗi byte.

| Tham số | Giá trị |
|---|---:|
| `W3E1_BAUD_BPS` | 460.800 bit/s |
| `W3E1_PAYLOAD_LEN_BYTES` | 59 B |
| `W3E1_FRAME_RATE_HZ` | 67 khung/s |
| `W3E1_STRESS_DURATION_MIN` | 10 phút = 600 s |

| Phần khung | Offset | Kích thước | Giá trị |
|---|---:|---:|---|
| Header — SYNC (byte đồng bộ) | 0 | 1 B | `0xAA` |
| Header — SEQ (Sequence Number) | 1 | 4 B | `uint32_t` little-endian, bắt đầu từ 0 |
| Header — LEN (Payload Length) | 5 | 2 B | `uint16_t` little-endian, giá trị 59 |
| Payload | 7 | 59 B | `(0x55u + seq + i) & 0xFFu` |
| CRC32 (Cyclic Redundancy Check 32-bit) | 66 | 4 B | `uint32_t` little-endian, `zlib.crc32(payload) & 0xFFFFFFFF`, tính riêng trên payload |

Header theo `<BIH` dài 7 B. CRC32 theo `<I`. Parser dùng SYNC, LEN và CRC32 để xác định khung. Ngắt đường rảnh chỉ báo thời điểm chuyển byte từ vòng DMA sang Stream Buffer. Một khung có thể qua nhiều lần đọc, và một lần đọc có thể chứa nhiều khung.

```sh
python tools/uart_stress.py --port COMx --baud 460800 --payload-size 59 --rate-hz 67 --duration-sec 600
```

`COMx` là cổng của USB–UART. Tham số được truyền tường minh vì mặc định của script khác bài giao và `--params-h` chỉ có thể đổi baud. `ser.read(4096)` có timeout 0,1 s và vòng gửi dùng `next_send += period`. Đọc chặn và jitter của host có thể làm số khung thực tế lệch mục tiêu hoặc làm các lần gửi dồn lại.

## 2. Băng thông và thời điểm ngắt

| Đại lượng | Biểu thức | Dự đoán |
|---|---|---:|
| Kích thước khung | `frame_bytes = 1 + 4 + 2 + 59 + 4` | 70 B |
| Tốc độ byte đỉnh khi dây bận | `wire_rate_Bps = W3E1_BAUD_BPS / 10` | 46.080 B/s |
| Lưu lượng khung trung bình theo nhịp đặt | `frame_rate_Bps = frame_bytes * W3E1_FRAME_RATE_HZ` | 4.690 B/s = 46.900 bit/s tính cả bit UART |
| Lưu lượng payload | `payload_rate_Bps = 59 * 67` | 3.953 B/s |
| Tỷ lệ thời gian dây bận | `wire_busy_pct = 100.0 * frame_rate_Bps / wire_rate_Bps` | 10,18% |
| Một ký tự UART | `char_us = 1000000.0 / wire_rate_Bps` | 21,701 µs |
| Thời gian truyền khung | `frame_ms = 1000.0 * frame_bytes / wire_rate_Bps` | 1,519 ms |
| Chu kỳ khung lý tưởng | `period_ms = 1000.0 / W3E1_FRAME_RATE_HZ` | 14,925 ms |
| Khoảng dây rảnh lý tưởng | `gap_ms = period_ms - frame_ms` | 13,406 ms |
| Tổng danh định trong 600 s | `frames_target = 67 * 600`, `bytes_target = frames_target * 70` | 40.200 khung, 2.814.000 B |

Với vòng DMA (Direct Memory Access) 256 B, ngắt Half-Transfer (HT) và Transfer-Complete (TC) luân phiên mỗi 128 byte nhận được. IDLE là sự kiện USART (Universal Synchronous/Asynchronous Receiver/Transmitter) phát hiện đường nhận rảnh.

| Sự kiện | Dữ liệu liền trên dây | Khung cách đều ở 67 Hz |
|---|---:|---:|
| HT → TC hoặc TC → HT tiếp theo | `1000.0 * 128 / 46080` = 2,778 ms | `2.778 + gaps * 13.406` ms, `gaps` bằng 1 hoặc 2 tùy pha (16,184 hoặc 29,590 ms), trung bình `1000.0 * 128 / 4690` = 27,292 ms |
| Hai HT liên tiếp hoặc hai TC liên tiếp | `1000.0 * 256 / 46080` = 5,556 ms | Trung bình `1000.0 * 256 / 4690` = 54,584 ms |
| USART IDLE | Không lặp khi luồng không có khoảng rảnh đủ dài | Cờ dự kiến lên khoảng 21,701 µs sau ký tự cuối, khoảng 1,541 ms sau đầu khung. Hai sự kiện IDLE cách nhau khoảng 14,925 ms |

Khoảng từ HT/TC đến IDLE phụ thuộc vị trí của byte chạm mốc 128 B trong khung, nên không có một chu kỳ chung cho cả ba nguồn. Nhịp ngắt danh định là `4690.0 / 128` = 36,64 HT/TC mỗi giây cộng khoảng 67 IDLE mỗi giây. Khi dữ liệu nối liền, nhịp HT/TC lên 360/s và IDLE chỉ báo khi dòng dữ liệu dừng đủ lâu. Tham chiếu [RM0008 của ST](https://www.st.com/resource/en/reference_manual/rm0008-stm32f103xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).

## 3. Kích thước bộ đệm và điều kiện 0 mất byte

Đặt hai ngân sách thiết kế cần đo ở Chốt 2. Với ngắt nhận, tối đa **500 µs** từ khi có sự kiện tới khi sao chép hết đoạn đã chốt, kể cả thời gian bị che ngắt, ngắt ưu tiên cao hơn và chép vòng. Với Parser, tối đa **5 ms** không đọc trong kịch bản host gửi dồn ở tốc độ dây. Khi bắt đầu một khoảng chờ, giả định Stream Buffer còn tối đa **70 B** tồn từ lần phục vụ trước. Sau mỗi khoảng chờ, Parser phải rút backlog về mức này trước khi lại gặp khoảng chờ xấu nhất, với thông lượng xử lý dài hạn lớn hơn lưu lượng vào.

| Khoản | Công thức C | Kết quả |
|---|---|---:|
| Byte mới trong ngân sách ngắt | `irq_extra = (46080u * 500u + 999999u) / 1000000u` | 24 B |
| Vòng DMA tối thiểu với mốc nửa vòng | `dma_min = 2u * (irq_extra + 1u)` | 50 B |
| Vòng DMA chọn | `dma_bytes = 256u`, `dma_half = dma_bytes / 2u` | 256 B, mốc 128 B |
| Byte có thể còn chờ trong DMA khi Parser ngừng đọc | `dma_pending_max = dma_half + irq_extra` | 152 B |
| Byte mới ở tốc độ dây trong 5 ms | `arrivals_5ms = (46080u * 5u + 999u) / 1000u` | 231 B |
| Stream Buffer tối thiểu ở tốc độ dây trong 5 ms | `stream_min = 70u + dma_pending_max + arrivals_5ms` | 453 B hữu dụng, mảng tối thiểu 454 B |
| Stream Buffer chọn | `stream_storage = 512u`, `stream_capacity = stream_storage - 1u` | 511 B hữu dụng, dư 58 B so với mốc 5 ms |
| Ngưỡng đánh thức | `trigger_bytes = 1u` | Đưa Parser từ Blocked sang Ready khi đã ghi ít nhất 1 B |

Ở mốc HT hoặc TC, nửa vòng còn lại phải chứa được toàn bộ byte đến trong thời gian phục vụ, và vẫn phải còn ít nhất 1 ô để con trỏ vòng phân biệt rỗng với đầy. Với lựa chọn 256 B, nửa vòng cho khoảng 2,778 ms ở tốc độ dây, lớn hơn ngân sách 0,500 ms. Cận 50 B là cận toán học của mô hình ngắt, tương ứng mốc 25 B chỉ có khoảng 0,543 ms để phục vụ và sinh khoảng 1.843 ngắt DMA/s khi dây bận liên tục. Lựa chọn 256 B giảm tần suất ngắt và cho thêm biên xử lý. Khung 70 B có thể đi qua ranh giới DMA nhờ Parser giữ trạng thái qua các lần đọc.

Ở Stream Buffer, cộng cả byte đã nằm trong DMA lúc Parser ngừng đọc vì ngắt có thể đẩy chúng sang Stream Buffer trong cùng cửa sổ. Mã `xStreamBufferGenericCreateStatic()` và `xStreamBufferSpacesAvailable()` trong kernel của repo giữ một ô trống. Khởi tạo `xStreamBufferCreateStatic(512u, 1u, storage, &stream_control)` với mảng 512 B cho 511 B hữu dụng. Khi Stream Buffer chứa dữ liệu, Parser đọc theo số byte trả về và tiếp tục vét dữ liệu đã sẵn sàng.

| Kiểm tra độ nhạy | Tính toán | Kết luận với mảng 512 B |
|---|---|---|
| Parser trễ 50 ms, host phát khung cách đều | Cửa sổ 50 ms có thể cắt tối đa 4 khung vì `50.0 + 1.519 < 4.0 * 14.925`. Dự trù `70 + 152 + 4 * 70 = 502` B. | Còn 9 B theo giả định phát đều. |
| Parser trễ 50 ms, host gửi dồn ở tốc độ dây | `70 + 152 + (46080 * 50 + 999) / 1000 = 2526` B. | Cần tối thiểu 2.527 B mảng nếu vẫn dùng ngân sách trễ này khi dây bận liên tục. |

Hai mảng dùng `256 + 512 = 768` B, bằng `100.0 * 768 / (20 * 1024)` = **3,75%** SRAM. Cộng dự trù khoảng 64 B cho điều khiển Stream Buffer và bộ đếm đường nhận thì khoảng 832 B (**4,06%**). Stack, khối điều khiển task, vùng làm việc của Parser và 1 KiB dự trù cho MSP (Main Stack Pointer) trong linker cần tính riêng theo file map của bản build.

### Điều kiện cho dự đoán

- Hai trình phục vụ ngắt dùng chung một đường ghi, cùng mức ưu tiên preemption hợp lệ với FreeRTOS để không chen nhau khi đọc điểm ghi và chép dữ liệu. Chuẩn hóa điểm ghi bằng `write_pos = (dma_bytes - remaining) % dma_bytes`, với `remaining` lấy từ `CNDTR` (Channel Number of Data to Transfer Register, thanh ghi số phần tử DMA còn phải chuyển). Khi vòng qua cuối mảng, chép đoạn cuối rồi đoạn đầu. Hiệu vị trí bằng 0 chỉ có nghĩa là không có byte mới khi đã bảo đảm chưa bỏ qua cả vòng.
- Xóa cờ đường rảnh theo trình tự đọc `USART1->SR` (Status Register, thanh ghi trạng thái) rồi `USART1->DR` (Data Register, thanh ghi dữ liệu) theo RM0008. Với khung phát đều còn khoảng 13,385 ms từ cờ IDLE tới đầu khung kế tiếp. Khi host tạo khoảng rảnh chỉ vừa đủ sinh IDLE, cần kiểm chứng thời gian xóa cờ và tránh việc CPU đọc tranh byte mới với DMA. Đo với khoảng nghỉ ngắn nhất vẫn tạo IDLE, kiểm tra thao tác đọc dữ liệu hoàn tất trước khi byte kế tiếp sẵn sàng và đối chiếu tổng byte nhận. Ngân sách 500 µs ở trên chỉ dùng để tính chống ghi đè vòng.
- Kiểm tra số byte thực trả về từ mỗi `xStreamBufferSendFromISR()`. Ghi nhận lần ghi thiếu và số byte hụt, theo dõi mức đầy cao nhất và lỗi USART/DMA. Thiếu byte, tràn vòng hoặc lỗi ngoại vi đều làm lần chạy thất bại. Giữ báo cáo ở ưu tiên thấp hơn Parser và đo khoảng cách phục vụ xấu nhất ngay khi bật báo cáo.
- **Dự đoán 600 s** với các cận trên được duy trì và đường nhận không lỗi: 0 byte mất, 0 lỗi CRC32, 0 khung mất. So tổng khung thật đã gửi với tổng khung hợp lệ và tổng byte nhận sau khi Parser vét xong. Kiểm cả dãy số thứ tự đầu/cuối, vì chỉ đếm khoảng nhảy giữa hai khung sẽ bỏ sót phần đầu hoặc cuối. Nếu firmware không echo, script có thể in PASS khi `received == 0`; đối chiếu bằng bộ đếm trên board. Với `sent` khung hợp lệ từ host, kỳ vọng `frames_ok == sent` và `bytes_received == sent * 70`.

## 4. Dự đoán tải CPU

Mô hình dùng 72 MHz từ [platform/system.c](../../platform/system.c) và lưu lượng danh định 4.690 B/s. Phương án đối chiếu nhận từng byte bằng ngắt USART RXNE (Read Data Register Not Empty, thanh ghi nhận có dữ liệu), ghi một byte vào cùng kiểu Stream Buffer và giữ ngưỡng đánh thức 1 B. Giả định Parser ưu tiên cao, đọc và ngủ lại sau mỗi lần được đánh thức. Tần suất gọi Parser trong mô hình DMA lấy xấp xỉ cận trên theo số sự kiện `67.0 + 4690.0 / 128`. Trùng nguồn ngắt hoặc gộp nhiều lần ghi có thể giảm số lần thức.

| Hạng mục | Ngân sách giả định |
|---|---:|
| Parser, gồm đọc byte, máy trạng thái và CRC32 phần mềm | 110 chu kỳ/B + 180 chu kỳ/khung |
| Đọc một lô và quay về chờ của Parser | 450 chu kỳ/lô, tính trong Parser |
| Ngắt nhận với DMA | 220 chu kỳ/sự kiện + 12 chu kỳ/B chép |
| Ngắt nhận từng byte | 300 chu kỳ/B, gồm đọc ngoại vi và gửi byte vào Stream Buffer |
| Scheduler và tick | 600 chu kỳ/cặp chuyển ngữ cảnh lúc thức/ngủ + 180 chu kỳ/tick ở 1 kHz |

```c
parser_cycles_per_sec = 4690 * 110 + 67 * 180 + wake_hz * 450;
dma_irq_cycles_per_sec = dma_event_hz * 220 + 4690 * 12;
byte_irq_cycles_per_sec = 4690 * 300;
kernel_cycles_per_sec = wake_hz * 600 + 1000 * 180;
busy_pct = 100.0 * (parser_cycles_per_sec + irq_cycles_per_sec + kernel_cycles_per_sec) / 72000000.0;
idle_pct = 100.0 - busy_pct;
```

| Đại lượng | DMA + IDLE + HT/TC | Ngắt RXNE từng byte |
|---|---:|---:|
| Sự kiện ngắt nhận/s | ~103,64 | 4.690 |
| Parser Task | 0,80% | 3,66% |
| Xử lý ngắt nhận | 0,11% | 1,95% |
| Tick và chuyển ngữ cảnh | 0,34% | 4,16% |
| CPU bận | **1,24%** | **9,78%** |
| FreeRTOS Idle Task | **98,76%** | **90,22%** |

Các phần trăm là ước lượng từ ngân sách chu kỳ tự đặt. Tổng được tính trước khi làm tròn và chưa tính định dạng/in báo cáo hay overhead nhỏ của timer đo. Idle Task là tác vụ rảnh của FreeRTOS, còn USART IDLE chạy trong trình phục vụ ngắt. Ở phương án RXNE, một ký tự mới hoàn tất sau khoảng 21,701 µs khi dây bận liên tục, nên việc đọc thanh ghi dữ liệu cần đáp ứng hạn riêng trước byte tiếp theo.

Khi triển khai, dùng TIM2 độc lập với SysTick để thu FreeRTOS run-time stats. TIM2 16 bit ở 100 kHz quay vòng sau `65536.0 / 100000.0` = 0,65536 s. Mở rộng bộ đếm lên 32 bit bằng đếm tràn và đọc nhất quán cả lúc tràn đang chờ xử lý. Trong 600 s, bộ đếm mở rộng tăng `100000u * 600u` = 60.000.000 tick. Thu snapshot bằng `uxTaskGetSystemState()` với vùng tĩnh và tính tỷ lệ từ hiệu hai snapshot. Kernel tính thời gian theo thời điểm chuyển task nên thời gian ngắt có thể được tính vào task bị ngắt. Đo chu kỳ trình phục vụ ngắt riêng khi cần so trực tiếp với bảng. Tham chiếu [datasheet STM32F103C8 của ST](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf).

## 5. Phân chia triển khai cho Chốt 2 : Phần em triển khai coding 

Tại Chốt 1, giao diện [uart_dma_rx.h](include/uart_dma_rx.h) đã có sẵn. `week3/ex1/include/FreeRTOSConfig.h` và target `w3e1` chưa có. Mẫu [week2/ex1](../../week2/ex1) dùng tên `snake_case`, comment `/* STEP ... */`, thanh ghi CMSIS (Cortex Microcontroller Software Interface Standard) và cấp phát tĩnh. Dự kiến giữ các quy ước này cùng cờ `-Wall -Wextra -Werror`.

| File | Vai trò dự kiến |
|---|---|
| `week3/ex1/uart_dma_rx.c` | Cấu hình PA10/USART1 và DMA1 Channel 5, vòng nhận, hai handler ngắt và Stream Buffer. Cung cấp các hàm nhận và lấy bộ đếm theo giao diện hiện có. |
| `week3/ex1/uart_frame_parser.c` | Máy trạng thái SYNC/SEQ/LEN/Payload/CRC32, kiểm dữ liệu và cập nhật bộ đếm khung. Parser Task gọi `uart_dma_rx_parse_once()`. |
| `week3/ex1/main.c` và `include/FreeRTOSConfig.h` | Task Parser và task lấy mẫu tải CPU với stack tĩnh, Idle Task tĩnh, cấu hình ngắt và run-time stats. |
| `week3/ex1/runtime_stats.c` | TIM2 độc lập và đọc bộ đếm mở rộng phục vụ run-time stats. |
| `platform/startup.c` và `CMakeLists.txt` | Nối vector DMA1 Channel 5 và USART1 hiện trỏ tới handler mặc định. Thêm target `w3e1` với `stream_buffer.c` và cấu hình FreeRTOS tĩnh. |
