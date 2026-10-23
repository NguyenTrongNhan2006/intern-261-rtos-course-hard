# W1.4 REPORT: Cấp phát tĩnh và Ngân sách RAM trên STM32F103

MSSV: 2452882. STM32F103 (20 KiB SRAM).
Trình biên dịch: Arm GNU GCC 14.2.1 (-O2 -g3, -Wall -Wextra -Werror).
Dữ liệu thực nghiệm: [measurements/](measurements/).

## 1. Tham số thí nghiệm phân mảnh heap_4

Kịch bản thực hiện theo tham số tự sinh của MSSV 2452882 trong `params.h`. Mỗi khối được cộng 8 byte tiêu đề `BlockLink_t` và căn chỉnh bội số 8 byte (`portBYTE_ALIGNMENT = 8`).

| Khối | Payload (bytes) | Header (bytes) | Padding (bytes) | Heap thực chiếm (bytes) |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 320 | 8 | 0 | 328 |
| 1 | 128 | 8 | 0 | 136 |
| 2 | 56 | 8 | 0 | 64 |
| 3 | 24 | 8 | 0 | 32 |
| 4 | 200 | 8 | 0 | 208 |
| 5 | 96 | 8 | 0 | 104 |
| **Tổng** | **824** | **48** | **0** | **872** |

Thứ tự cấp phát: 0 → 1 → 2 → 3 → 4 → 5. Thứ tự giải phóng: 5 → 4 → 2 → 1 → 0 → 3.
Thí nghiệm lặp lại 6 chu kỳ (tổng 72 thao tác). Sai số ngân sách RAM cho phép so với file `.map` là 123 byte.

## 2. Dữ liệu thực nghiệm phân mảnh heap_4

Dữ liệu log đầy đủ được lưu tại [measurements/heap_log_20261016_192528.csv](measurements/heap_log_20261016_192528.csv), gồm đúng 72 dòng thao tác, chia làm 6 chu kỳ đồng nhất và kết thúc bằng `# W1E4 HEAP DONE`. Dung lượng ban đầu là $F_0 = 8184$ byte (heap 8192 byte trừ 8 byte cấu trúc kết thúc `xEnd`).

### Bảng diễn biến chi tiết chu kỳ đầu (Cycle 0)

| Bước | Thao tác | Khối | Free hiện tại (bytes) | Minimum-ever (bytes) | Khối trống lớn nhất (bytes) | Số khối trống |
| ---: | :--- | ---: | ---: | ---: | ---: | ---: |
| 1 | ALLOC | 0 | 7856 | 7856 | 7856 | 1 |
| 2 | ALLOC | 1 | 7720 | 7720 | 7720 | 1 |
| 3 | ALLOC | 2 | 7656 | 7656 | 7656 | 1 |
| 4 | ALLOC | 3 | 7624 | 7624 | 7624 | 1 |
| 5 | ALLOC | 4 | 7416 | 7416 | 7416 | 1 |
| 6 | ALLOC | 5 | 7312 | 7312 | 7312 | 1 |
| 7 | FREE | 5 | 7416 | 7312 | 7416 | 1 |
| 8 | FREE | 4 | 7624 | 7312 | 7624 | 1 |
| 9 | FREE | 2 | 7688 | 7312 | 7624 | 2 |
| 10 | FREE | 1 | 7824 | 7312 | 7624 | 2 |
| 11 | FREE | 0 | 8152 | 7312 | 7624 | 2 |
| 12 | FREE | 3 | 8184 | 7312 | 8184 | 1 |

Từ chu kỳ 1 đến chu kỳ 5, các giá trị dung lượng trống, khối trống lớn nhất và số lượng khối trống lặp lại hoàn toàn đồng nhất với chu kỳ 0. `Minimum-ever` giữ nguyên mốc cực tiểu lịch sử 7312 byte ($8184 - 872$).

### Phân tích định lượng cơ chế phân mảnh
Cấp phát tuần tự sáu khối chiếm tổng cộng 872 byte ($824 + 48$), đưa dung lượng trống chạm đáy $8184 - 872 = 7312$ byte. Khi giải phóng khối 5 rồi khối 4, hai khối này tiếp giáp ngay với vùng trống cuối heap nên thuật toán coalescing của `heap_4` gộp liền mạch thành một khối trống lớn (số khối trống vẫn là 1). Khi giải phóng khối 2, khối 3 vẫn đang chiếm dụng nên ngăn cách vùng nhớ 64 byte vừa giải phóng với vùng trống phía sau, tạo ra một hòn đảo bộ nhớ cô lập và đẩy số khối trống lên 2. Tiếp tục giải phóng khối 1 và khối 0 sẽ gộp liên tiếp với khối 2 thành một vùng trống phía trước rộng 528 byte ($64 + 136 + 328$). Tại bước 11, tổng dung lượng trống là 8152 byte nhưng khối trống lớn nhất chỉ đạt 7624 byte, thể hiện hiện tượng phân mảnh ngoại vi tạm thời (yêu cầu cấp phát lớn hơn 7624 byte sẽ thất bại dù tổng bộ nhớ còn thừa). Tại bước 12, giải phóng khối 3 sẽ dỡ bỏ vật cản, giải thuật coalescing hợp nhất cả hai phía thành một khối trống duy nhất 8184 byte. Hệ thống không bị phân mảnh tồn dư sau mỗi chu kỳ.

## 3. Chuyển dịch sang Cấp phát tĩnh 100%

Toàn bộ hệ thống được chuyển đổi sang `configSUPPORT_STATIC_ALLOCATION = 1` và `configSUPPORT_DYNAMIC_ALLOCATION = 0`. Mọi đối tượng được tạo bằng `xTaskCreateStatic`, `xQueueCreateStatic`, `xSemaphoreCreateMutexStatic`. Hai callback cung cấp bộ nhớ cho kernel `vApplicationGetIdleTaskMemory` và `vApplicationGetTimerTaskMemory` được triển khai đầy đủ. 

Kiểm tra `tools/check_api.sh` đạt kết quả PASS tuyệt đối trên toàn bộ mã nguồn. File nhị phân ELF của cả ba target không còn bất kỳ ký hiệu nào của `pvPortMalloc`, `vPortFree` hay mảng `ucHeap`. Cả ba firmware đều vượt qua tự kiểm tra khởi tạo `# QUEUE_MUTEX_SELFTEST,PASS`.

## 4. Đo đạc và Tinh chỉnh kích thước Stack

Sau khi đo đạc baseline ban đầu (độ sâu stack 256 words cho user tasks và timer), căn cứ vào mực nước đỉnh `uxTaskGetStackHighWaterMark()`, stack của các tác vụ người dùng và timer được tinh chỉnh giảm xuống 128 words, Idle giữ nguyên 128 words.

| Target | Tác vụ | Kích thước (words) | Chưa dùng (words) | Đã dùng (words) | Vùng trống (bytes) |
| :--- | :--- | ---: | ---: | ---: | ---: |
| **w1e4 (RTA)** | RTA0 | 128 | 98 | 30 | 392 |
| | RTA1 | 128 | 98 | 30 | 392 |
| | RTA2 | 128 | 98 | 30 | 392 |
| | RTA3 | 128 | 98 | 30 | 392 |
| | Idle | 128 | 80 | 48 | 320 |
| | Timer | 128 | 96 | 32 | 384 |
| **w1e4_blinky** | BlinkA | 128 | 100 | 28 | 400 |
| | BlinkB | 128 | 100 | 28 | 400 |
| | Idle | 128 | 96 | 32 | 384 |
| | Timer | 128 | 96 | 32 | 384 |
| **w1e4_trace** | TraceA | 128 | 98 | 30 | 392 |
| | TraceB | 128 | 76 | 52 | 304 |
| | Idle | 128 | 88 | 40 | 352 |
| | Timer | 128 | 96 | 32 | 384 |

Tác vụ tiêu thụ stack lớn nhất trong toàn bộ hệ thống là TraceB với 52 words (còn dư 76 words tương đương 304 byte). Tất cả các tác vụ đều thỏa mãn biên an toàn tối thiểu 32 words (`# STACK_MARGIN_32_WORDS,1`).

Bằng chứng UART sau tinh chỉnh:
- [static_rta_tuned_20261016_200818.csv](measurements/static_rta_tuned_20261016_200818.csv)
- [static_blinky_tuned_20261016_200954.csv](measurements/static_blinky_tuned_20261016_200954.csv)
- [static_trace_tuned_20261016_201024.csv](measurements/static_trace_tuned_20261016_201024.csv)

## 5. Ngân sách RAM tính tay và Đối chiếu Linker Map

Bảng hạch toán áp dụng cho target `w1e4` (APP 3) sau tinh chỉnh với `sizeof(StaticTask_t) = 96` và `sizeof(StaticQueue_t) = 80`.

| Nhóm đối tượng | Cách tính tay | Kích thước (bytes) |
| :--- | :--- | ---: |
| Stack 4 task ứng dụng | 4 × 128 words × 4 bytes | 2048 |
| Stack Idle task | 128 words × 4 bytes | 512 |
| Stack Timer daemon | 128 words × 4 bytes | 512 |
| Sáu khối TCB | 6 × 96 bytes | 576 |
| Task handles và indices | 2 mảng × 4 tasks × 4 bytes | 32 |
| Bảng stack high-water mark | 6 × 4 bytes | 24 |
| Control block Queue và Mutex | 2 × 80 bytes | 160 |
| Storage Queue ứng dụng | 4 phần tử × 4 bytes | 16 |
| Queue và Mutex handles | 2 × 4 bytes | 8 |
| Module calibration | 4 tasks × 16 bytes | 64 |
| Trạng thái report, hyperperiod, completed | 3 × 4 bytes | 12 |
| Bộ đệm tracer W1.2 | 256 sự kiện × 8 bytes + 4 scalar × 4 bytes | 2064 |
| Biến SystemCoreClock | 1 × 4 bytes | 4 |
| Biến FreeRTOS scheduler | 9 List_t × 20 bytes + 14 scalar 4-byte | 236 |
| Biến FreeRTOS timer | 2 List_t × 20 + 5 scalar + 1 Queue_t (80) + 4 msg (48) | 188 |
| Biến port ARM_CM3 và padding | 4 + 1 + 4 bytes + 3 bytes padding | 12 |
| **Tổng section RAM tính tay** | **Toàn bộ đối tượng (.data + .bss)** | **6468** |

### Đối chiếu với file map thực tế (`build/w1e4-tuned/w1e4.map`)

| Phân vùng | Tính tay (bytes) | Thực tế từ Map (bytes) | Chênh lệch (bytes) |
| :--- | ---: | ---: | ---: |
| `.data` | 8 | 8 | 0 |
| `.bss` | 6460 | 6464 | +4 |
| `.noinit` | 0 | 0 | 0 |
| **Tổng section RAM** | **6468** | **6472** | **+4** |
| MSP dự trữ (`_Min_Stack_Size`) | 1024 | 1024 | 0 |
| **Tổng RAM gồm MSP** | **7492** | **7496** | **+4** |

Chênh lệch giữa thực tế và tính tay là 4 byte, phát sinh do trình biên dịch chèn căn chỉnh 8-byte cho các mảng stack trong `.bss`. Sai số 4 byte hoàn toàn thỏa mãn giới hạn cho phép 123 byte (`W1E4_RAM_BUDGET_TOLERANCE_BYTES`). SRAM chưa sử dụng còn lại là $20480 - 7496 = 12984$ byte.

### Hiệu quả thu hồi RAM qua tinh chỉnh Stack

| Target | RAM baseline map (bytes) | RAM sau tinh chỉnh map (bytes) | Tiết kiệm được (bytes) | Tổng gồm MSP (bytes) |
| :--- | ---: | ---: | ---: | ---: |
| **w1e4** | 9032 | 6472 | 2560 (5 stack × 512B) | 7496 |
| **w1e4_blinky** | 4632 | 3096 | 1536 (3 stack × 512B) | 4120 |
| **w1e4_trace** | 6728 | 5192 | 1536 (3 stack × 512B) | 6216 |

## 6. Tính tin cậy của minh chứng thực nghiệm

Bộ dữ liệu đã loại bỏ các bản chụp lỗi không hợp lệ (lượt thu RTA lúc 19:42:18 chứa byte NUL và lượt thu Blinky lúc 20:09:01 thiếu phần header). Các bản chụp còn lại đều có marker hoàn tất, không có byte NUL, chuỗi 254 sự kiện tracer liên tục với `overflow = 0`. Các log baseline và tuned được lưu giữ minh bạch trong thư mục `measurements/`.

## 7. Kết luận

Thí nghiệm chứng minh cơ chế cấp phát động `heap_4` gây ra phân mảnh ngoại vi tạm thời do trật tự giải phóng khối bị cô lập, nhưng tự hồi phục hoàn toàn sau khi giải phóng hết khối cản. Việc chuyển dịch sang cấp phát tĩnh 100% loại bỏ vĩnh viễn nguy cơ phân mảnh và sụp đổ bộ nhớ khi vận hành, mang lại tính tất định thời gian $O(1)$. Kỹ thuật giám sát mực nước đỉnh ngăn xếp bằng `uxTaskGetStackHighWaterMark()` cung cấp bằng chứng định lượng vững chắc để thu hồi 2560 byte RAM an toàn, đưa sai số giữa bảng tính toán lý thuyết và linker map thực tế về mức 4 byte, hoàn toàn làm chủ ngân sách 20 KiB SRAM trên STM32F103.
