# W4.2 – REPORT (HIỆN THỰC HỆ THỐNG MIXED-CRITICALITY)

- **MSSV**: `2452882` | **MCU**: `STM32F103C8T6` (`72 MHz`, `20 KiB SRAM`) | **Ngày đo**: `21/11/2026`
- **SSOT**: `week4/ex1/include/mixed_criticality_design.h`, `week4/ex1/REPORT.md`, [PREDICTION.md](PREDICTION.md), [include/mixed_criticality_system.h](include/mixed_criticality_system.h), `include/params.h`.
- **Bằng chứng thực nghiệm (`week4/ex2/measurements/`)**:
  - **Logic Analyzer `1.000.000 bps` chuẩn SSOT (`24 MHz`, `416,67 ms`)**: [Sigrok session.sr](measurements/Sigrok%20session.sr) (`PA4`, `PB6/PB7`, `PA9` — `41/41` khung `W4E2` giải mã ở `1.000.000 bps`, `0` lỗi CRC32).
  - **Logic Analyzer `STOP mode` (`1 MHz`, `5.000 ms`)**: [w4e2_stop_1mhz.sr](measurements/w4e2_stop_1mhz.sr) (`PA4`, `PA0` trong chu trình `quiesce on` `1,52 s` → `quiesce off`).
  - **Log UART dài hạn (`921.600 bps` do CP2102 bản cũ `REV_0100` không hỗ trợ `1 Mbps`)**: [w4e2_capture.csv](measurements/w4e2_capture.csv) (`3.095` khung, `33,04 s`), [w4e2_capture_1mbps.csv](measurements/w4e2_capture_1mbps.csv) (`3.059` khung, `30,96 s`, uptime `25,9 phút`) và [w4e2_steady_24mhz.sr](measurements/w4e2_steady_24mhz.sr) (`42/42` khung, `0` lỗi CRC32).

---

## 1. Số đo

### 1.1. Bảng Priority, Stack High-Water Mark & Ngân sách RAM

*(Đo định kỳ qua `uxTaskGetStackHighWaterMark()`; `1 word = 4 bytes`; ngưỡng dung sai/cảnh báo `W4E2_STACK_MARGIN_TOLERANCE_WORDS = 23 words`.)*

| Task | Prio (`W4E2`/`W4E1`) | Budget W4.1 (w / B) | HWM dự đoán (w) | HWM đo được (w / B) | Đỉnh dùng (w / B) | Chênh lệch (`đo − dự đoán`) | Kết luận |
|---|---:|---:|---:|---:|---:|---:|---|
| `control` | 8 / 8 | 192 / 768 | 120 | **149** / 596 | **43** / 172 | `+29` *(dư hơn)* | Đạt (`stack_low = 0`) |
| `imu` | 7 / 7 | 192 / 768 | 95 | **118** / 472 | **74** / 296 | **`+23`** *(đúng biên)* | Đạt (**margin nhỏ nhất**) |
| `telemetry` | 5 / 5 | 192 / 768 | 115 | **142** / 568 | **50** / 200 | `+27` *(dư hơn)* | Đạt (`stack_low = 0`) |
| `shell` | 3 / 3 | 256 / 1.024 | 160 | **194** / 776 | **62** / 248 | `+34` *(dư hơn)* | Đạt (`stack_low = 0`) |
| `idle_aux` | 2 / 2 | 192 / 768 | 100 | **135** / 540 | **57** / 228 | `+35` *(dư hơn)* | Đạt (**margin nhỏ thứ 2**) |

- **RAM tĩnh (`build/w4e2/w4e2.map`)**: `.data = 20 B`, align `= 4 B`, `.bss = 8.576 B`, `_Min_Stack_Size (MSP) = 1.024 B` → **Tổng `9.624 B`** `<= 11.776 B` (thiết kế W4.1) `<= 15.684 B` (`W4E1_RAM_BUDGET_BYTES`, dư `6.060 B`). `FLASH = 24.348 B` (`37,15%`).

### 1.2. CPU Load, Timing Phần cứng & Chức năng

- **CPU load đo được (run-time stats `TIM2` 100 kHz) khi ổn định**: **`7,13%`** (lần 1 đoạn sạch `18,15 s`, `7%–8%`) và **`8,00%`** (lần 2 sạch hoàn toàn `30,96 s`, `3.055` mẫu `8%` + `4` mẫu `7%`; lệnh `stats`: `RT%(aux/sh/tel/imu/ctl/idle/tmr) = 0/0/0/3/3/92/0`, `hb_miss = 0`, `low = 0`) `<< 76%` (`W4E1_CPU_BUDGET_PERCENT_MAX`).

| Hạng mục kiểm chứng | Kết quả đo thực tế (UART + Logic Analyzer) | Ngưỡng SSOT W4.1/W4.2 | Trạng thái |
|---|---|---|---|
| **UART `1.000.000 bps` (`PA9`)** | Đo tại `24 MHz`: bit `24 samples = 1,000 µs` (`1.000.000 bps`), `41/41` khung `W4E2` `80 B` (`100 Hz`), `0` lỗi CRC32 (`+ 6.154` khung UART log `0` lỗi CRC) | `1.000.000 bps`, `0` lỗi | **PASS** |
| **Control `1 kHz` (`PA4` + `DWT`)** | Logic Analyzer: `T = 996,79 / 1.000,00 / 1.003,21 µs` (`PA4` HIGH `15,92 µs` avg). `DWT`: `jitter_max = 22,56–29,61 µs`, `wcrt_max = 39,35–46,97 µs`, `overrun = 0` | `jitter <= 113 µs`, `WCRT <= 97 µs`, `0` overrun | **PASS** |
| **`I2C1` 400 kHz (`PB6/PB7`)** | `209` burst `500 Hz` (`T = 1.999,74 µs`), xung `SCL = 393,72 kHz` (`2,540 µs`), burst `14 B = 393,17 µs`; lệch pha sau `PA4` đúng `42,18 µs`, **`0` overlap với `PA4`** | `~400 kHz`, `500 Hz`, không chạm `control` | **PASS** |
| **Shell runtime (`<= 200 ms`)** | `status`, `stats`, `set telemetry 200 → 10` (`dt = 200 ms`, `OK`), `quiesce on/off` (`OK`), `rx_faults = 0` / `tx_errors = 0` (trong cửa sổ sạch) | Timeout `<= 200 ms` | **PASS** |
| **Tickless `STOP mode` (`PA0`)** | Trong `quiesce on` (`1,52 s`): `PA4` tắt hẳn, `PA0` kéo `LOW` `31` nhịp (`tổng 1.389,54 ms = 91,3%` thời gian ngủ, mỗi nhịp tới `47,38 ms`); `quiesce off` phục hồi `1 kHz` tức thời (`stop_faults = 0`) | Vào STOP `>= 5 ticks`, `0` fault | **PASS** |

- **`tools/check_api.sh` kết quả**: **PASS** (`check_api_python` trên `week4` & `platform`: `0` HAL/LL/CMSIS-OS, `0` API động; `arm-none-eabi-nm build/w4e2/w4e2.elf` có `0` symbol `pvPortMalloc`/`ucHeap`/`_sbrk`/`printf`/`__aeabi_d*`; build `-O2 -g3 -Wall -Wextra -Werror -Wdouble-promotion` **0 warning, 0 error**).

---

## 2. Giải thích sai lệch (ĐỊNH LƯỢNG)

1. **Stack HWM (`HWM_đo` cao hơn dự đoán `+23` đến `+35 words` — dư an toàn hơn)**:
   - Thứ hạng khớp hoàn toàn [PREDICTION.md](PREDICTION.md): **`imu` có margin nhỏ nhất (`118/192 w`, dùng `74 w`, đúng biên `23 w`)**, tiếp theo là **`idle_aux` (`135/192 w`, dùng `57 w`)**, rộng nhất là **`shell` (`194/256 w`)**.
   - Chênh lệch âm phần dùng thực tế do: (a) `idle_aux` đọc trực tiếp `ulTaskGetRunTimeCounter()` thay vì `uxTaskGetSystemState()` (`-35 w`); (b) mọi buffer lớn (`command[128]`, `reply[128]`, `telemetry_frame[20]`) đặt tĩnh trong `.bss` và không dùng `snprintf`/`sscanf`/`libm`; (c) `-O2` tối ưu gọn PID 1 trục ở `control` (`-29 w`) và CRC32 ở `telemetry` (`-27 w`). Giữ nguyên ngân sách W4.1 để chừa biên cho W4.3.

2. **CPU Load (`7,13%–8,00%` thực đo vs `20,24%` dự đoán LO-Mode, chênh `-12,24%` đến `-13,11%`)**:
   - Dự đoán W4.1 lấy cận bảo thủ: thực đo bằng `DWT` + `TIM2` cho thấy `control` chỉ mất `~3%` (`~30 µs / 1.000 µs` vs `7%`), `imu` mất `~3%` (task `Blocked` suốt `~393 µs` chờ bus I2C1 400 kHz, phần ISR + scale `float` chỉ `~60 µs / 2.000 µs` vs `8,25%`), `telemetry` `< 1%` (`DMA1_Ch4` không giữ CPU), `shell`/`idle_aux` `~0%` khi không gõ lệnh, phần dư `~1–2%` là nền ISR `SysTick` + `TIM2` (khớp `2,10%` dự đoán).

3. **Bằng chứng cô lập lỗi I2C1 trong thực nghiệm**:
   - Trong [w4e2_capture.csv](measurements/w4e2_capture.csv), `2` đợt lỏng dây I2C1 (`t = 769,3 s` và `771,3 s`) kích `+31` lần `recover_bus()` (`imu_pulses = 1` tại `row 1833`) và `+922` chu kỳ safe-hold (`stale`), nhưng **`control_overrun_count` giữ nguyên `0`** và **`jitter_us` không đổi (`22,61 µs <= 113 µs`)**, chứng minh recovery chạy trong ngữ cảnh `imu` (P7, mở ngắt) + SPSC lock-free cô lập hoàn toàn vòng `1 kHz`.

---

## 3. Trả lời câu hỏi trade-off

**Vì sao `control_task` dùng `TIM3` Compare + Direct Notification thay vì `vTaskDelay` / `vTaskDelayUntil`?**

1. **Loại bỏ sai số lượng tử & trễ `SysTick` (NVIC `15`)**: `vTaskDelay(1)` bị sai số lượng tử tới `±1.000 µs`; `vTaskDelayUntil` đánh thức mọi task cùng cạnh tick `1 ms`, bị kẹt sau danh sách `xTaskIncrementTick()` và bị mọi ISR ngoại vi (NVIC `6–7`) trì hoãn `SysTick`. `TIM3` (`1 MHz`, lệch pha `+500 µs`, NVIC `5`) kết hợp `vTaskNotifyGiveFromISR` đánh thức thẳng `control` (P8) không qua queue/list.
2. **Bảo toàn cơ chế Tickless Idle (`configUSE_TICKLESS_IDLE = 2`)**: Nếu `control` nằm trong delayed list mỗi `1 tick`, `xExpectedIdleTime` không bao giờ đạt ngưỡng `5 ticks` để ngủ sâu. Nhờ chờ `ulTaskNotifyTake(..., portMAX_DELAY)`: lúc chạy `1 kHz`, cổng **Adaptive Sleep Gating** giữ `SLEEP WFI` (`< 2 µs`); khi `quiesce on` tắt `TIM3`, MCU hạ xuống **`STOP LPDS`** (`91,3%` thời gian ngủ đo được trên `PA0`).
3. **Chứng minh bằng số đo thực tế**: Trên Logic Analyzer (`24 MHz`), chu kỳ `PA4` giữ `996,79–1.003,21 µs`, `I2C1` khởi chạy lệch pha sau `PA4` đúng `42,18 µs` (`0` overlap); `DWT` ghi nhận `jitter_max = 22,56 µs` (ổn định) / `29,61 µs` (đỉnh sau thoát `STOP`) `<< 113 µs` và **`0` lần overrun**.
