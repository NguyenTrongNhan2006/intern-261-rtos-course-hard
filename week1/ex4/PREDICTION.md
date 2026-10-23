# W1.4 PREDICTION: Phân mảnh Heap và Ngân sách RAM tĩnh

MSSV: 2452882. Soạn trước khi thực hiện đo đạc trên STM32F103.

## 1. Dự đoán phân mảnh heap_4

Heap cấu hình `configTOTAL_HEAP_SIZE = 8192` byte. Sau khi khởi tạo cấu trúc quản lý danh sách khối trống (trừ 8 byte cho cấu trúc kết thúc `xEnd`), dung lượng trống ban đầu danh định là $F_0 = 8184$ byte.

### Bảng tính toán kích thước khối
Trên kiến trúc ARM Cortex-M3 (port GCC ARM_CM3), tiêu đề `BlockLink_t` gồm một con trỏ 4-byte và một trường `size_t` 4-byte, suy ra `xHeapStructSize = 8` byte. Yêu cầu căn chỉnh `portBYTE_ALIGNMENT = 8`. Kích thước thực tế bị khấu trừ khỏi heap tuân theo công thức `align_up(payload + 8, 8)`.

| Khối | Payload (bytes) | Header (bytes) | Padding (bytes) | Heap thực chiếm (bytes) |
| :--- | ---: | ---: | ---: | ---: |
| 0 | 320 | 8 | 0 | 328 |
| 1 | 128 | 8 | 0 | 136 |
| 2 | 56 | 8 | 0 | 64 |
| 3 | 24 | 8 | 0 | 32 |
| 4 | 200 | 8 | 0 | 208 |
| 5 | 96 | 8 | 0 | 104 |
| **Tổng** | **824** | **48** | **0** | **872** |

Mọi payload đều chia hết cho 8 nên không có padding. Tổng heap thực chiếm cực đại là 872 byte.

### Diễn biến dự đoán chu trình 12 bước (vòng lặp 6 lần, tổng 72 thao tác)
Thứ tự cấp phát: 0 → 1 → 2 → 3 → 4 → 5. Thứ tự giải phóng: 5 → 4 → 2 → 1 → 0 → 3.

| Bước | Thao tác | Khối | Free dự kiến (bytes) | Min-ever dự kiến (bytes) | Khối trống lớn nhất (bytes) | Số khối trống |
| ---: | :--- | ---: | ---: | ---: | ---: | ---: |
| 1 | ALLOC | 0 | $F_0 - 328 = 7856$ | 7856 | 7856 | 1 |
| 2 | ALLOC | 1 | $F_0 - 464 = 7720$ | 7720 | 7720 | 1 |
| 3 | ALLOC | 2 | $F_0 - 528 = 7656$ | 7656 | 7656 | 1 |
| 4 | ALLOC | 3 | $F_0 - 560 = 7624$ | 7624 | 7624 | 1 |
| 5 | ALLOC | 4 | $F_0 - 768 = 7416$ | 7416 | 7416 | 1 |
| 6 | ALLOC | 5 | $F_0 - 872 = 7312$ | 7312 | 7312 | 1 |
| 7 | FREE | 5 | $F_0 - 768 = 7416$ | 7312 | 7416 | 1 |
| 8 | FREE | 4 | $F_0 - 560 = 7624$ | 7312 | 7624 | 1 |
| 9 | FREE | 2 | $F_0 - 496 = 7688$ | 7312 | 7624 | 2 |
| 10 | FREE | 1 | $F_0 - 360 = 7824$ | 7312 | 7624 | 2 |
| 11 | FREE | 0 | $F_0 - 32 = 8152$ | 7312 | 7624 | 2 |
| 12 | FREE | 3 | $F_0 = 8184$ | 7312 | 8184 | 1 |

### Cơ chế phân mảnh ngoại vi và hồi phục
Sau bước 8, khối 4 và 5 được giải phóng và gộp liền kề (coalescing) với vùng trống cuối heap, tạo thành khối trống 7624 byte. Tại bước 9, việc giải phóng khối 2 tạo ra một vùng trống 64 byte bị cô lập ở phía trước do khối 3 (32 byte) vẫn đang chiếm dụng, ngăn cách với vùng trống phía sau. Các bước 10 và 11 giải phóng tiếp khối 1 và 0, gộp liên tiếp với khối 2 tạo thành vùng trống phía trước rộng 528 byte ($64 + 136 + 328$). Tại bước 11, tổng bộ nhớ trống là 8152 byte nhưng khối trống lớn nhất chỉ là 7624 byte; đây là phân mảnh ngoại vi tạm thời (2 khối trống). Tại bước 12, giải phóng khối 3 sẽ xóa bỏ vật cản, thuật toán coalescing của `heap_4` hợp nhất khối trước (528 byte), khối 3 (32 byte) và khối sau (7624 byte) thành một khối duy nhất rộng 8184 byte ($F_0$). Số byte mất vĩnh viễn do phân mảnh sau chuỗi thử là 0 byte. Bộ đếm `xPortGetMinimumEverFreeHeapSize()` giữ nguyên mốc cực tiểu lịch sử là 7312 byte.

## 2. Dự toán ngân sách RAM tĩnh và tinh chỉnh Stack

Chuyển đổi toàn bộ hệ thống sang `configSUPPORT_STATIC_ALLOCATION = 1` và `configSUPPORT_DYNAMIC_ALLOCATION = 0`. Mọi đối tượng (TCB, Stack, Queue, Mutex) được cấp phát tĩnh trong `.bss` hoặc `.data`. Sai số cho phép so với file `.map` là `W1E4_RAM_BUDGET_TOLERANCE_BYTES = 123` byte.

### Bảng dự toán RAM đối tượng tĩnh (Target w1e4 APP 3 sau tinh chỉnh)
Sau khi đo đạc baseline (stack 256 words cho user tasks và timer) và nhận thấy mức tiêu thụ đỉnh cao nhất chỉ 52 words, cấu hình được tinh chỉnh giảm stack user tasks và timer từ 256 xuống 128 words, Idle giữ nguyên 128 words.

| Nhóm đối tượng | Cách tính tay | RAM dự toán (bytes) |
| :--- | :--- | ---: |
| Stack 4 task ứng dụng | 4 × 128 words × 4 bytes | 2048 |
| Stack Idle task | 128 words × 4 bytes | 512 |
| Stack Timer daemon | 128 words × 4 bytes | 512 |
| Sáu khối TCB | 6 × 96 bytes (`sizeof(StaticTask_t)`) | 576 |
| Task handles và indices | 2 mảng × 4 tasks × 4 bytes | 32 |
| Bảng stack high-water mark | 6 × 4 bytes | 24 |
| Control block Queue và Mutex | 2 × 80 bytes (`sizeof(StaticQueue_t)`) | 160 |
| Storage Queue ứng dụng | 4 phần tử × 4 bytes | 16 |
| Queue và Mutex handles | 2 × 4 bytes | 8 |
| Biến module calibration | 4 tasks × 16 bytes | 64 |
| Trạng thái report, hyperperiod, completed | 3 × 4 bytes | 12 |
| Bộ đệm tracer W1.2 | 256 sự kiện × 8 bytes + 4 scalar × 4 bytes | 2064 |
| Biến SystemCoreClock | 1 × 4 bytes | 4 |
| Biến toàn cục FreeRTOS scheduler | 9 List_t × 20 bytes + 14 scalar 4-byte | 236 |
| Biến toàn cục FreeRTOS timer | 2 List_t × 20 + 5 scalar + 1 Queue_t (80) + 4 msg (48) | 188 |
| Biến port ARM_CM3 và padding dự trù | 4 + 1 + 4 bytes + 3 bytes padding | 12 |
| **Tổng section RAM dự toán** | **Cộng toàn bộ đối tượng (.data + .bss)** | **6468** |
| MSP dành riêng theo linker | `_Min_Stack_Size = 0x400` | 1024 |
| **Tổng RAM dự toán gồm MSP** | **6468 + 1024** | **7492** |
| SRAM còn dư so với 20 KiB | $20480 - 7492$ | 12984 |

### Dự kiến biến động RAM giữa các cấu hình

| Target | RAM baseline map (bytes) | Tiết kiệm dự kiến (bytes) | RAM sau tinh chỉnh map (bytes) |
| :--- | ---: | ---: | ---: |
| w1e4 (RTA) | 9032 | 5 × 128 × 4 = 2560 | 6472 |
| w1e4_blinky | 4632 | 3 × 128 × 4 = 1536 | 3096 |
| w1e4_trace | 6728 | 3 × 128 × 4 = 1536 | 5192 |

Chênh lệch giữa map thực tế (6472 byte) và tính tay (6468 byte) dự kiến là 4 byte do căn chỉnh mảng stack trong `.bss`, hoàn toàn nằm trong biên độ sai số 123 byte.
