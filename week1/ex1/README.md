# W1.1 – Bring-up bare-metal

## Mục tiêu
Viết tay toàn bộ chuỗi khởi động STM32F103C8T6 từ con số 0 và chạy FreeRTOS blinky 2 task.

## Đọc trước
- RM0008: chương 3 (memory/bus), chương 4 (Embedded Flash – latency), chương 7 (RCC – HSE/PLL),
  chương 9 (GPIO/AFIO), chương 15 (TIM2-5 – xuất PWM/so sánh để tạo xung 1 kHz)
- PM0056: chương 2 (exception model, vector table, priority grouping NVIC)
- Tài liệu GNU ld: *Scripts* (MEMORY, SECTIONS)
- Tài liệu CMake: *cross compiling* (toolchain file), `CMakePresets.json` schema
- *Mastering the FreeRTOS Real Time Kernel* chương 1–2

## Việc phải làm
1. `cmake/arm-none-eabi.cmake`, root `CMakeLists.txt`, `CMakePresets.json` (target `w1e1` + `w1e1_flash` qua OpenOCD).
2. Linker script `platform/linker/stm32f103c8.ld`: FLASH 64K/RAM 20K, `.data`/`.bss`/`.noinit`, stack ở đỉnh RAM.
3. Startup: vector table đầy đủ, `Reset_Handler` (copy `.data` từ flash, xóa `.bss`), gọi `SystemInit` rồi `main`.
4. Clock: HSE 8 MHz + PLL ×9 → 72 MHz bằng thanh ghi RCC, flash latency 2 wait-state trước khi tăng xung.
5. Driver GPIO thanh ghi (CRL/CRH/ODR/BSRR/BRR) và map `SVC_Handler`/`PendSV_Handler`/`SysTick_Handler` sang port FreeRTOS.
6. Cấu hình NVIC priority group theo `configPRIO_BITS`; `PendSV` phải là priority **thấp nhất**.
7. Dùng TIM xuất xung so sánh 1 kHz ra 1 chân GPIO để xác minh 72 MHz bằng logic analyzer.
8. Implement `include/blinky.h`: 2 task toggle chân tag ở `W1E1_TASK_A/B_PERIOD_MS` (xem `params.h` sau khi chạy `gen_params.py`).
9. Viết `SETUP.md` giải thích từng thanh ghi đã ghi (RCC, FLASH, GPIO CRL/CRH, NVIC).

## Sản phẩm nộp
- Build sạch `-Wall -Wextra -Werror`, 2 target CMake (`w1e1`, `w1e1_flash`).
- Ảnh logic analyzer đo xung 1 kHz xác minh tần số.
- `SETUP.md`.

## Tiêu chí đạt
Giải thích được từng dòng linker script, vì sao cần flash wait-state ở 72 MHz, và vì sao `PendSV` phải có priority thấp nhất trong hệ.

## Giờ dự kiến
6 giờ.
