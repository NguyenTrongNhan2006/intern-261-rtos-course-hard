# W4.2 – REPORT (HIỆN THỰC HỆ THỐNG MIXED-CRITICALITY)

- **MSSV**: `2452882` | **MCU**: `STM32F103C8T6` (`72 MHz`, `20 KiB SRAM`) | **Ngày đo**: `21/11/2026`
- **Hồ sơ đối chiếu (SSOT)**: `week4/ex1/include/mixed_criticality_design.h`, `week4/ex1/REPORT.md`, [PREDICTION.md](PREDICTION.md), [include/mixed_criticality_system.h](include/mixed_criticality_system.h), `include/params.h`.
- **Dữ liệu thực nghiệm (2 file log đo liên tiếp trên cùng phiên boot, uptime `> 25 phút`)**:
  - **Lần lưu 1** — [measurements/w4e2_capture.csv](measurements/w4e2_capture.csv) (`3.095` khung `W4E2` `80 B`, `33,04 s`, `tick_ms = 751.178 → 784.222 ms`, `0` lỗi CRC32): kiểm chứng chạy ổn định, `2` đợt nhiễu tiếp xúc I2C1 (`+31` recovery, `overrun = 0`), đổi runtime `set telemetry 200 → 10`, và `STOP mode` (`+52` lần vào STOP, `stop_faults = 0`).
  - **Lần lưu 2** — [measurements/w4e2_capture_1mbps.csv](measurements/w4e2_capture_1mbps.csv) (`3.059` khung `W4E2` `80 B`, `30,96 s`, `tick_ms = 1.525.592 → 1.556.552 ms`, `0` lỗi CRC32): **100% chạy ổn định sạch hoàn toàn** (`0` recovery I2C mới, `0` stale mới, `0` rx_fault mới, `0` overrun), kiểm chứng lặp lại `HWM` không rò rỉ sau `25,9 phút` hoạt động và `set telemetry 200` (`2` khung `200 ms`) → `10 ms`.
- **Ghi chú điều kiện đo thực tế**:
  - Mã nguồn mặc định và file biên dịch hiện tại `build/w4e2/w4e2.elf` đã giữ đúng `W4E1_TELEMETRY_BAUD_BPS = 1.000.000 bps` (`BRR = 72`, `W4E2_UART_BAUD_OVERRIDE=""`, không sửa `params.h`).
  - Khi thu hai file log trên bằng adapter **CP2102 bản cũ (`VID_10C4&PID_EA60&REV_0100`)** bị giới hạn phần cứng tự ép mọi yêu cầu `1.000.000` về `921.600 bps`, MCU chạy ảnh nạp tạm ở `921.600 bps` (`BRR = 78`, sai số `+0,16%`); tên file `w4e2_capture_1mbps.csv` phản ánh cờ `--baud 1000000` trên host nhưng đường truyền vật lý qua CP2102 cũ vẫn chạy ở `921.600 bps` (do chưa chạy `w4e2_flash`, `tick_ms` nối tiếp liên tục từ `784 s` sang `1.525 s`). Khi dùng adapter CH340 / FT232RL / CP2102N hoặc đo Logic Analyzer `PA9`, nạp bản `1.000.000 bps` mặc định bằng `cmake --build build/w4e2 --target w4e2_flash`.

---

## 1. Số đo

### 1.1. Bảng Priority & Stack High-Water Mark thực đo so với thiết kế W4.1 và Dự đoán W4.2

*(Đo định kỳ qua `uxTaskGetStackHighWaterMark()` trong `idle_aux_task`; `1 word = 4 bytes`; ngưỡng cảnh báo/sai số `W4E2_STACK_MARGIN_TOLERANCE_WORDS = 23 words`.)*

| Task (`role`) | Priority (`W4E2` / `W4E1`) | Stack budget W4.1 (words / B) | HWM dự đoán `PREDICTION.md` (words) | High-water-mark đo được (words / B) | Đỉnh thực dùng `Budget − HWM` (words / B) | Chênh lệch `HWM_đo − HWM_dự_đoán` (words) | Đánh giá biên an toàn (`>= 23 w`) |
|---|---:|---:|---:|---:|---:|---:|---|
| `control` | 8 / 8 | 192 / 768 | 120 | **149** / 596 | **43** / 172 | `+29` *(dư hơn dự đoán)* | Đạt (`stack_low = 0`) |
| `imu` | 7 / 7 | 192 / 768 | 95 | **118** / 472 | **74** / 296 | **`+23`** *(đúng biên 23 w)* | Đạt (**margin nhỏ nhất**, đúng dự đoán) |
| `telemetry` | 5 / 5 | 192 / 768 | 115 | **142** / 568 | **50** / 200 | `+27` *(dư hơn dự đoán)* | Đạt (`stack_low = 0`) |
| `shell` | 3 / 3 | 256 / 1.024 | 160 | **194** / 776 | **62** / 248 | `+34` *(dư hơn dự đoán)* | Đạt (`stack_low = 0`) |
| `idle_aux` | 2 / 2 | 192 / 768 | 100 | **135** / 540 | **57** / 228 | `+35` *(dư hơn dự đoán)* | Đạt (**margin nhỏ thứ 2**, đúng dự đoán) |

- **Ngân sách RAM tĩnh đối chiếu từ `build/w4e2/w4e2.map`**:
  - `.data = 20 B`, alignment gap `= 4 B`, `.bss = 8.576 B`, `.noinit = 0 B` → RAM tĩnh section `= 8.600 B`.
  - Cộng `_Min_Stack_Size = 1.024 B` (MSP) → **Tổng RAM tĩnh + MSP = `9.624 B`** (`<= 11.776 B` thiết kế W4.1 và `<= 15.684 B` ngân sách `W4E1_RAM_BUDGET_BYTES`, còn dư **`6.060 B`** so với ngân sách và **`10.856 B`** so với 20 KiB SRAM). `FLASH = 24.348 B / 64 KiB` (`37,15%`).

### 1.2. CPU Load, Timing `control`, Shell & Tickless `STOP mode` (đối chiếu cả 2 lần đo)

- **CPU load đo được (run-time stats dùng `TIM2` 100 kHz) khi hệ thống chạy ổn định**:
  - **Lần đo 2 sạch hoàn toàn** (`w4e2_capture_1mbps.csv`, `3.059` khung, `30,96 s`, `0` recovery I2C): trung bình **`8,00%`** (`3.055` mẫu `8%`, `4` mẫu `7%`), `hb_miss = 0`, `low = 0`.
  - **Lần đo 1** (`w4e2_capture.csv`, đoạn `18,15 s` đầu ổn định): trung bình **`7,13%`** (`7% – 8%`, phân rã lệnh `stats`: `RT%(aux/sh/tel/imu/ctl/idle/tmr) = 0/0/0/3/3/92/0`; toàn cửa sổ `33,04 s` trung bình `7,05%`).
  - Tổng hợp: CPU ổn định **`7% – 8%`** (`<< 76% = W4E1_CPU_BUDGET_PERCENT_MAX`, dư biên **`~68%`**).

| Thông số kiểm chứng | Lần đo 1 (`w4e2_capture.csv`, `33,04 s`) | Lần đo 2 (`w4e2_capture_1mbps.csv`, `30,96 s`) | Ngưỡng SSOT W4.1 / W4.2 | Kết luận |
|---|---:|---:|---:|---|
| Số khung telemetry `W4E2` (`80 B`) / lỗi CRC32 | `3.095` / **`0`** | `3.059` / **`0`** | `0` lỗi CRC | **PASS** |
| Chu kỳ phát telemetry danh định (`dt_ms`) | `10 ms` (`3.091` bước) | `10 ms` (`3.056` bước) | `10 ms` (`100 Hz`) | **PASS** |
| Đổi chu kỳ runtime (`set telemetry 200` → `10`) | `1` khung `200 ms` → `10 ms` (`OK`) | `2` khung `200 ms` → `10 ms` (`OK`) | Đổi runtime `<= 200 ms` | **PASS** |
| CPU load ổn định (min / avg / max) | `7% / 7,13% / 8%` *(đoạn sạch)* | **`7% / 8,00% / 8%`** *(toàn bộ `30,96 s`)* | `<= 76%` | **PASS** |
| Jitter cực đại `control` (`DWT`) | `22,61 µs` *(ổn định)* / **`29,61 µs`** | **`29,61 µs`** *(giữ nguyên sau `25,9 phút`)* | `<= 113 µs` (báo động `100 µs`) | **PASS** |
| WCRT cực đại `control` (`DWT`) | `45,85 µs` / **`46,97 µs`** | **`46,97 µs`** *(giữ nguyên sau `25,9 phút`)* | `<= 97 µs` (`R_HI` W4.1) | **PASS** |
| `control_overrun_count` / `hb_miss` / `stack_low` | **`0`** / **`0`** / **`0`** | **`0`** / **`0`** / **`0`** | `0` | **PASS** |
| Biến thiên `imu_rec` / `stale` / `rx_faults` / `tx_errors` trong cửa sổ | `+31` / `+924` *(2 đợt nhiễu dây)* / `0` / `0` | **`0`** / **`0`** / **`0`** / **`0`** | `0` khi dây bus ổn định | **PASS** |
| Tickless `STOP mode` (`quiesce on` → wake → `quiesce off`) | `stop`: `147 → 199` (**`+52` lần STOP**), `stop_faults = 0` | `stop = 199` *(không tăng thêm trong CSV)*, `stop_faults = 0` | Vào STOP `>= 5 ticks`, `0` fault | **PASS** |
| Stack HWM `(ctl/imu/tel/sh/aux)` (words) | `149 / 118 / 142 / 194 / 135` | **`149 / 118 / 142 / 194 / 135`** *(khớp tuyệt đối)* | `>= 23 w`, không trôi | **PASS** |

- **`tools/check_api.sh` kết quả**: **PASS** (`0` vi phạm HAL/LL/CMSIS-OS, `0` API động `*Create`/`pvPortMalloc`, kiểm chứng tương đương bằng bộ lọc Python `tools/check_w4e1.py::check_api_python` trên `week4` và `platform` do môi trường Windows dùng `py` thay vì `python3` trong Git Bash; đồng thời `arm-none-eabi-nm build/w4e2/w4e2.elf` xác nhận **`0`** symbol `pvPortMalloc`, `vPortFree`, `ucHeap`, `malloc`, `_sbrk`, `printf`, `snprintf`, `__aeabi_d*`; build `-O2 -g3 -Wall -Wextra -Werror -Wdouble-promotion` **0 warning, 0 error**).

---

## 2. Giải thích sai lệch (ĐỊNH LƯỢNG)

### 2.1. Sai lệch Stack High-Water Mark (`HWM_đo` cao hơn `HWM_dự_đoán` từ `+23` đến `+35 words`)
- **Định hướng thứ hạng khớp tuyệt đối với [PREDICTION.md](PREDICTION.md)**: `imu` có HWM nhỏ nhất (`118/192 w`, tiêu thụ đỉnh `74 w`), theo sát là `idle_aux` (`135/192 w`, dùng `57 w`), trong khi `shell` vẫn còn trống rộng nhất (`194/256 w`) nhờ `command[128]` và `reply[128]` cấp phát tĩnh trong `.bss`.
- **Nguồn gốc chênh lệch theo hướng an toàn hơn dự đoán** (`control +29 w`, `imu +23 w`, `telemetry +27 w`, `shell +34 w`, `idle_aux +35 w`):
  1. **`idle_aux` (`57 w` thực dùng vs `92 w` dự đoán, `-35 w`)**: Hiện thực trong `mixed_criticality_system.c` gọi trực tiếp `ulTaskGetRunTimeCounter()` cho từng handle thay vì cấp khung cục bộ và duyệt danh sách của `uxTaskGetSystemState()`, đồng thời không gọi hàm format `sprintf`.
  2. **`shell` (`62 w` vs `96 w`, `-34 w`)**: Parser và `reply_writer_t` được compiler ARM GCC 14.2.1 (`-O2`) inline gọn, không dùng `sscanf`/`snprintf`, buffer `2 × 128 B` nằm hoàn toàn trong `.bss`.
  3. **`control` (`43 w` vs `72 w`, `-29 w`) & `telemetry` (`50 w` vs `77 w`, `-27 w`)**: Luật điều khiển PID 1 trục gọn (`accel_g[0]`, `gyro_dps[1]`, `clamp_control`) không gọi hàm lượng giác `libm`, còn `telemetry_frame[20]` là mảng `static` trong `.bss`.
  4. **`imu` (`74 w` vs `97 w`, đúng biên `+23 w`)**: Đã trải qua thực tế `70` lần `recover_bus()` trên board mà đỉnh tiêu thụ vẫn dừng ở `74 w` (`HWM = 118 w`).
- **Quyết định giữ nguyên ngân sách stack W4.1 (`192w / 256w`)**: Giữ khớp 100% với SSOT `W4E1_RAM_BUDGET_TABLE` đã duyệt và chừa đủ biên an toàn cho các hook đo WCRT/fault-injection ở W4.3 (tổng RAM vẫn chỉ `9.624 / 15.684 B`).

### 2.2. Sai lệch CPU Load (`7,13% – 8,00%` ổn định thực đo vs `20,24%` dự đoán LO-Mode)
Chênh lệch **`-12,24%` đến `-13,11%`** (tải thực thấp hơn dự đoán ~`2,5× – 2,8×`) đến từ việc dự đoán W4.1 lấy cận trên bảo thủ cho RTA, cụ thể từng thành phần:
1. **`control` (`~3%` thực đo vs `7,00%` dự đoán, chênh `-4,00%`)**: WCRT cực đại đo bằng `DWT` là `45,85–46,97 µs` (trung bình `~30 µs / 1.000 µs = 3%`) thay vì `C_LO = 70 µs`, do PID trong W4.2 không dùng phép lọc góc lượng giác nặng.
2. **`imu` (`~3%` thực đo vs `8,25%` dự đoán, chênh `-5,25%`)**: Trong `~433 µs` truyền burst `14 B` ở `I2C1 400 kHz`, task `imu` ở trạng thái `Blocked` (0% CPU); phần CPU chạy 19 lần `I2C1_EV_IRQHandler` + nhân nghịch đảo `float` ở `-O2` thực tế chỉ tốn `~60 µs / 2.000 µs = 3%` (thấp hơn `C_LO = 165 µs`).
3. **`telemetry` (`< 1%` vs `1,80%`, chênh `-1,2%`)**: `DMA1_Ch4` truyền nền không giữ CPU; CRC32 16-entry cho `76 B` ở `-O2` chỉ tốn `< 80 µs / 10.000 µs < 0,8%`.
4. **`shell` (`~0%` vs `0,84%`) & `idle_aux` (`~0%` vs `0,25%`)**: Khi không gõ lệnh liên tục `shell` ngủ `50 ms/lần`; `idle_aux` chạy `100 ms/lần`. Phần còn lại `~1–2%` (`7–8% − 6%`) khớp với nền ISR `SysTick` + `TIM2` (`~2,10%` dự đoán).

### 2.3. Đối chiếu 2 lần đo & sự kiện nhiễu bus `I2C1` (bằng chứng cô lập lỗi thực tế)
- Ở **lần đo 1** ([w4e2_capture.csv](measurements/w4e2_capture.csv)), sau `18,15 s` đầu chạy sạch (`0` recovery, `0` stale mới), xuất hiện 2 đợt gián đoạn tiếp xúc dây bus `I2C1` tại `t = 769,33–769,63 s` (`rows 1815..1845`) và `t = 771,30–771,68 s` (`rows 2012..2050`):
  - `imu_rec` tăng `39 → 70` (**`+31` lần recovery**), tại `row 1833` ghi nhận **`imu_pulses = 1`** (SDA bị giữ LOW và được `recover_bus()` nhả sau `1` xung SCL); `stale` tăng `1.293 → 2.215` (`+922` nhịp `control` chạy nhánh safe-hold).
  - Trong lúc `imu` lùi chu kỳ `20 ms` / chờ `IMU_STARTING 100 ms`, CPU load giảm từ `7–8%` xuống **`4–5%`** — xác nhận chéo trực tiếp tải thực của `imu` là `~3%`.
  - **Kết quả then chốt**: Xuyên suốt cả `31` lần recovery bus `I2C1`, **`control_overrun_count` giữ nguyên `0`** và **`jitter_us` không hề tăng (giữ nguyên `22,61 µs <= 113 µs`)**, chứng minh thực nghiệm rằng recovery ở ngữ cảnh task `imu` (P7, mở ngắt) và kênh SPSC lock-free hoàn toàn cô lập được vòng điều khiển `1 kHz`.
- Ở **lần đo 2** ([w4e2_capture_1mbps.csv](measurements/w4e2_capture_1mbps.csv), `30,96 s`, uptime `1.525 → 1.556 s`), dây bus ổn định hoàn toàn nên ghi nhận **`0` recovery mới, `0` stale mới, `0` rx_fault mới, `0` overrun**, CPU giữ đều **`8,00%`** và HWM 5 task giữ nguyên tuyệt đối `149 / 118 / 142 / 194 / 135 words` (không có hiện tượng rò rỉ hay phình stack sau `> 25 phút` chạy).

---

## 3. Trả lời câu hỏi trade-off

**Vì sao `control_task` dùng `TIM3` Compare (`+500 µs` lệch pha `SysTick`) + Direct-to-Task Notification thay vì `vTaskDelay` / `vTaskDelayUntil` theo tick?**

1. **Giới hạn lượng tử hóa và xung đột pha của `SysTick` (`vTaskDelay` / `vTaskDelayUntil`)**:
   - `SysTick` có độ phân giải `1 tick = 1.000 µs` và chạy ở **NVIC ưu tiên thấp nhất (`15`)**. Với `vTaskDelay(1)`, thời điểm gọi nằm giữa tick khiến sai số lượng tử có thể tới **`±1.000 µs`** (vượt gấp `~9×` trần jitter cứng `W4E1_CONTROL_LOOP_JITTER_MAX_US = 113 µs`).
   - Kể cả khi dùng `vTaskDelayUntil(&last, 1)`, mọi task định kỳ khác (`telemetry`, `shell`, `idle_aux`) đều được `SysTick_Handler` đánh thức tại **cùng một cạnh tick `t = k × 1.000 µs`**, cộng thêm toàn bộ thời gian duyệt delayed list của `xTaskIncrementTick()` và bị mọi ISR ngoại vi (`I2C1` NVIC 6, `USART1`/`DMA` NVIC 7) trì hoãn `SysTick` (NVIC 15).
   - Ngược lại, trong thiết kế hiện thực ([control_clock.c](control_clock.c)), `TIM3` chạy `1 MHz` (`PSC = 71`, `ARR = 999`, `CCR1 = 500`) đặt **lệch pha `+500 µs`** so với `SysTick` và gán **NVIC `5`** (ưu tiên cao nhất trong các ISR gọi FreeRTOS API), gọi `vTaskNotifyGiveFromISR` đánh thức trực tiếp `control` (P8) không qua queue/list.
   - **Chứng minh bằng số đo thực tế trên board**: Qua `6.154` khung đo (`3.095 + 3.059` khung, tổng uptime `> 25,9 phút`, gồm `31` lần recovery `I2C1` và `52` lần vào/thoát `STOP`), jitter cực đại của `control` chỉ đạt **`22,61 µs`** (ổn định) và **`29,61 µs`** (đỉnh sau thoát `STOP`) — **thấp hơn `3,8× – 5,0×` so với trần `113 µs`** (biên dư `83,39 µs`), `wcrt_max = 46,97 µs`, và **`0` lần overrun**.

2. **Tương thích với cơ chế Tickless Idle (`configUSE_TICKLESS_IDLE = 2`)**:
   - Nếu `control` dùng `vTaskDelayUntil(..., 1 tick)`, kernel luôn thấy tác vụ kế tiếp trong `pxDelayedTaskList` chỉ cách `<= 1 tick`, do đó `xExpectedIdleTime` **không bao giờ đạt ngưỡng `configEXPECTED_IDLE_TIME_BEFORE_SLEEP = 5`**, làm cơ chế Tickless Idle bị vô hiệu hóa hoàn toàn kể cả khi muốn hạ nguồn.
   - Khi `control` và `imu` chờ `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` tách rời khỏi danh sách delay theo tick:
     - Lúc vòng `1 kHz` đang chạy, cổng **Adaptive Sleep Gating** ([power.c](power.c)) kiểm tra `TIM3->CR1.CEN != 0` nên chỉ dùng **`SLEEP mode` (`WFI`, trễ `< 2 µs`)**, bảo toàn `PLL 72 MHz` và jitter `22,61 µs <= 113 µs`.
     - Khi hệ thống tạm ngưng vòng điều khiển (`quiesce on`, tắt `TIM3`), FreeRTOS lập tức tính ra `xExpectedIdleTime >= 5 ticks` và hạ xuống **`STOP mode` (`RTC LSI`)** thực thụ — đã kiểm chứng tăng **`+52` lần vào `STOP` (`147 → 199`)** trong `1,74 s` với **`stop_faults = 0`** và `overrun = 0` sau khi đánh thức.
