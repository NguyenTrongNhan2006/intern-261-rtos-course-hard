# FreeRTOS trên STM32F103 — Khóa tự học 4 tuần (Bản nâng cao)

> IoT Lab · Sinh viên năm 1–2 · ~16–18 giờ/tuần · tổng ~66 giờ
> **Viết tay từ thanh ghi. Không công cụ sinh code. Không HAL. Không CMSIS-OS.**

Bản nâng cao dành cho người muốn hiểu RTOS đến tận phần cứng: từ linker script, vector table,
cho đến phân tích lập lịch, driver ngắt, low-power và debug sau sự cố.

**Kết quả cuối khóa:** thiết kế một hệ thống mixed-criticality trên STM32F103, **chứng minh** đạt
deadline bằng phân tích (Response Time Analysis) và số đo, và chịu được fault injection.

---

## 1. Chuẩn bị

### Phần cứng
| Thiết bị | Ghi chú |
|---|---|
| STM32F103C8T6 (Blue Pill) | 72 MHz, 20 KB RAM, 64 KB Flash. Chip clone (CKS32/CS32) có thể khác errata |
| ST-Link V2 | Dùng với OpenOCD + GDB |
| USB-UART ≥ 1 Mbps (CH340 / CP2102) | Nối USART1 (PA9 TX, PA10 RX) |
| Logic analyzer 8 kênh 24 MHz | PulseView |
| Đồng hồ đo dòng | Đo ở nhánh 3.3 V; ngắt LED nguồn khi đo STOP mode |
| MPU6050, LED, 2 nút nhấn | |

### Phần mềm
- `arm-none-eabi-gcc`, `arm-none-eabi-gdb`, OpenOCD
- CMake ≥ 3.20, Ninja
- Python 3 (`pyserial`, `matplotlib`), PulseView

### Lấy mã nguồn
```bash
git clone --recurse-submodules <repo-url>
cd <repo>
python3 tools/gen_params.py <MSSV>      # sinh bộ tham số riêng của bạn
```
`gen_params.py` tạo `weekN/exM/include/params.h` và `PARAMS.md`. Mọi con số trong bài (task set, baud,
giới hạn jitter, độ trôi cho phép...) đều lấy từ đây. Một số MSSV sẽ nhận **task set không lập lịch được**,
và việc phát hiện ra điều đó là một phần của bài.

---

## 2. Bạn phải tự viết những gì

Được dùng sẵn **chỉ hai thứ**: header CMSIS-Core (`stm32f1xx.h`, `core_cm3.h`) và mã nguồn FreeRTOS-Kernel.
Mọi thứ còn lại bạn tự viết trong `platform/` ở bài W1.1 và dùng lại cho các bài sau:

| Thành phần | Nội dung |
|---|---|
| Linker script | FLASH/RAM, `.data` / `.bss` / `.noinit`, stack |
| Startup | Vector table, `Reset_Handler` (copy `.data`, xóa `.bss`) |
| Clock | HSE + PLL lên 72 MHz, flash latency – bằng thanh ghi RCC |
| Driver | GPIO, UART, I2C, DMA, TIM, RTC… – tất cả mức thanh ghi |
| Build | `cmake/arm-none-eabi.cmake`, `CMakeLists.txt`, `CMakePresets.json` |

Không đọc hiểu được từng dòng mình viết thì không qua được vấn đáp.

### Build, nạp, debug
```bash
cmake --preset debug
cmake --build --preset debug --target w1e1
cmake --build --preset debug --target w1e1_flash     # nạp qua OpenOCD
```

---

## 3. Quy tắc bắt buộc

| Quy tắc | Chi tiết |
|---|---|
| Chỉ API gốc FreeRTOS | Cấm `cmsis_os.h` / `cmsis_os2.h` và mọi hàm `os*` |
| Chỉ thanh ghi | Cấm `HAL_*`, `LL_*`. Lưu ý: CMSIS-Core (định nghĩa thanh ghi) **khác** CMSIS-OS (wrapper RTOS) |
| Không sinh code | Cấm CubeMX, wizard CubeIDE, project mẫu của hãng. Có file `.ioc` hoặc dòng `USER CODE BEGIN` là vi phạm |
| Build | Chỉ CMake + Ninja, viết tay. Không Makefile. Cờ `-Wall -Wextra -Werror` |
| Bộ nhớ | Từ bài W1.4 trở đi: `configSUPPORT_DYNAMIC_ALLOCATION = 0`, chỉ dùng API `*Static` |
| Dùng AI | Được phép. Ghi prompt vào `AI_LOG.md`. Không giải thích được dòng code mình nộp → vấn đáp = 0 |

Trước khi nộp:
```bash
./tools/check_api.sh        # phải in ra PASS
```
Script kiểm tra CMSIS-OS, HAL/LL, dấu vết code sinh tự động, Makefile/`.ioc` và cấp phát động từ W1.4.

---

## 4. Quy trình làm mỗi bài

1. Đọc `README.md` của bài và đúng phần tài liệu được chỉ định.
2. Điền `PREDICTION.md` (tính tay nếu bài yêu cầu) và **commit trước khi chạy**.
3. Hiện thực theo interface trong `include/`.
4. Đo: logic analyzer, tracer tự viết, DWT cycle counter, đồng hồ đo dòng.
5. Điền `REPORT.md`: số đo, bằng chứng, **phân tích sai số định lượng** so với dự đoán.
6. Vấn đáp: mentor sẽ đổi thông số hoặc tiêm lỗi tại chỗ.

---

## 5. Lộ trình

| Tuần | Bài | Chủ đề |
|---|---|---|
| **1** | ex1 | Bring-up bare-metal: linker, startup, clock 72 MHz, CMake – tất cả viết tay |
| | ex2 | Tracer tự viết (trace hook + DWT) và đo thời gian context switch |
| | ex3 | Phân tích lập lịch: Liu–Layland, Response Time Analysis, so với WCRT đo được |
| | ex4 | Phân mảnh heap, chuyển sang 100% cấp phát tĩnh, ngân sách RAM khớp file `.map` |
| **2** | ex1 | Driver I2C bằng ngắt, timeout, bus recovery, xử lý errata |
| | ex2 | Ring buffer lock-free (SPSC) vs queue |
| | ex3 | Priority inversion, chained blocking, deadlock; tự hiện thực Priority Ceiling |
| | ex4 | Event group barrier, task notification, tìm bug mất sự kiện |
| **3** | ex1 | UART DMA + IDLE line ở baud tới 1 Mbps, không mất byte |
| | ex2 | Tickless idle + STOP mode với RTC, đo dòng và độ trôi tick |
| | ex3 | HardFault handler + phân tích sau sự cố |
| | ex4 | Debug hộp đen: chỉ có file `.elf`, dùng GDB đọc cấu trúc kernel |
| **4** | ex1–4 | Hệ thống mixed-criticality: control loop 1 kHz, RTA, fault injection, bảo vệ |

Chi tiết từng bài: xem [`SYLLABUS.md`](SYLLABUS.md).

---

## 6. Chấm điểm

| Thành phần (mỗi bài) | Trọng số |
|---|---|
| Code | 25% |
| Số đo + phân tích | 40% |
| Vấn đáp | 35% |

| Tuần | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| Trọng số | 20% | 25% | 25% | 30% |

- **Đạt:** tổng ≥ 7/10 và không tuần nào dưới 6.
- **Điểm liệt:** CMSIS-OS / HAL / LL / file sinh tự động → phần code của bài đó = 0.
  Không có `AI_LOG.md` hoặc không giải thích được code mình nộp → phần vấn đáp = 0.
  Dùng heap từ W1.4 trở đi → trừ 50% phần code.

---

## 7. Cấu trúc repo

```
.
├── README.md
├── SYLLABUS.md              # nội dung chi tiết 16 bài
├── platform/                # bạn tự viết ở W1.1: startup, linker, clock, driver, CMake
├── third_party/FreeRTOS-Kernel/
├── tools/
│   ├── gen_params.py        # sinh tham số theo MSSV
│   ├── check_api.sh         # kiểm tra trước khi nộp
│   └── *.py                 # script phía PC (vẽ Gantt, stress UART...)
└── weekN/exM/
    ├── README.md            # đề bài
    ├── include/             # interface + params.h
    ├── PREDICTION.md
    ├── REPORT.md
    └── RUBRIC.md
```

## 8. Tài liệu

- *Mastering the FreeRTOS Real Time Kernel* + FreeRTOS API Reference
- freertos.org: *RTOS for ARM Cortex-M*, *Low Power Support*, *Trace Hook Macros*
- RM0008 (Reference manual STM32F1), PM0056 (Cortex-M3 programming manual), datasheet + errata sheet STM32F103x8/B
- Datasheet MPU6050 · Tài liệu GNU ld, CMake (toolchain file), GDB, OpenOCD
- Liu & Layland (1973) về Rate Monotonic; Response Time Analysis (Joseph & Pandya, 1986)

## 9. Dùng MCU khác?

Được, nhưng bạn phải tự port, vẫn dùng API gốc FreeRTOS và driver mức thanh ghi. Mentor chỉ hỗ trợ STM32F1.
