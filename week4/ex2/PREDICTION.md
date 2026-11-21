# W4.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

- **MSSV**: `2452882` | **MCU**: `STM32F103C8T6` (Cortex-M3 @ 72 MHz, 20 KiB SRAM) | **Ngày chốt**: `21/11/2026` *(trước khi viết bất kỳ file `.c` nào của W4.2)*
- **Nguồn đối chiếu (SSOT)**: `week4/ex1/include/mixed_criticality_design.h`, `week4/ex1/REPORT.md`, `week4/ex1/include/params.h`, `week4/ex2/include/params.h`, `week4/ex2/include/mixed_criticality_system.h`.
- **Giả định biên dịch & cấp phát**: `-O2 -g3`, `configSUPPORT_STATIC_ALLOCATION = 1`, `configSUPPORT_DYNAMIC_ALLOCATION = 0`, không gọi `printf`/`snprintf`/`sprintf` của newlib; mọi buffer lớn (`cmd_line`, `reply_buf`, DMA RX/TX, SPSC, `TaskStatus_t`) cấp phát tĩnh trong `.bss` đúng theo `W4E1_RAM_BUDGET_TABLE`.

---

## 1. Dự đoán priority/stack

- **Task dự đoán có `uxTaskGetStackHighWaterMark()` gần ngân sách nhất (margin còn trống nhỏ nhất)**: **`imu`** *(ngân sách `192 words = 768 B`, dự đoán tiêu thụ đỉnh `~97 words`, HWM còn trống `~95 words` — bám sát phía sau là **`idle_aux`** ở `192 words`, dự đoán HWM `~100 words`)*.
  - **Lý do kỹ thuật**:
    1. `shell` tuy có parser và định dạng chuỗi phản hồi nhưng được cấp ngân sách lớn nhất (**`256 words = 1.024 B`**) và cả `cmd_line` (128 B) lẫn `reply_buf` (128 B) đều cấp phát tĩnh trong `.bss` (mục 13 của `W4E1_RAM_BUDGET_TABLE`), không nằm trên stack, nên khoảng trống HWM còn lại của `shell` (`~160 words`) rộng hơn các task `192 words`.
    2. Trong 4 task cùng ngân sách `192 words`, **`imu`** có chuỗi gọi lồng sâu nhất ở cả nhánh thường lẫn nhánh sự cố: khung lưu ngữ cảnh PendSV (16 w) + buffer thô 14 B & `w4e2_imu_sample_t` 28 B trên stack (~11 w) + chuỗi `mpu6050_read_burst` -> `i2c1_master_read` -> `i2c1_transfer` -> `xSemaphoreTake` / `ulTaskNotifyTakeIndexed` và nhánh `i2c1_recover_locked` -> `i2c1_recovery_pulses` (~36 w) + phép đổi `int16 -> float` soft-ABI `libgcc` (~20 w) + `xEventGroupSetBits` (~14 w) ≈ **85–105 words** (cận bảo thủ W4.1 dự trù 110 w).
    3. **`idle_aux`** (`192 words`) đứng thứ hai do gọi `uxTaskGetSystemState()` + 5 lần `uxTaskGetStackHighWaterMark()` + chia nguyên 64-bit (`__aeabi_uldivmod`) + `tickless_rtc_get_stats()` ≈ **80–100 words**.
  - **Bảng dự đoán chi tiết 5 role** *(căn cứ biên đo `-O2` thực nghiệm W1.4: 30–52 w cho task đơn giản; ràng buộc `W4E2_STACK_MARGIN_TOLERANCE_WORDS = 23 words`)*:

| Role | Priority (`W4E2_TASK_PRIORITY`) | Stack budget W4.1 (words / B) | Dự đoán đỉnh sử dụng `-O2` (words) | Dự đoán `uxTaskGetStackHighWaterMark()` (words) | Ghi chú biên an toàn |
|---|---:|---:|---:|---:|---|
| `control` | 8 | 192 / 768 | 72 *(65–80)* | **120** *(112–127)* | Soft-float PID + SPSC read + EventGroup; dự kiến còn trống > 100 w |
| `imu` | 7 | 192 / 768 | 97 *(85–105)* | **95** *(87–107)* | **Margin nhỏ nhất** (chuỗi gọi I2C1 + recovery + soft-float) |
| `telemetry` | 5 | 192 / 768 | 77 *(65–85)* | **115** *(107–127)* | CRC32 bảng 16 phần tử + `DMA1_Ch4` TX Mutex; buffer phát là `.bss` |
| `shell` | 3 | 256 / 1.024 | 96 *(85–105)* | **160** *(151–171)* | Budget 256 w + `cmd_line`/`reply_buf` tĩnh (không dùng `snprintf`) |
| `idle_aux` | 2 | 192 / 768 | 92 *(80–100)* | **100** *(92–112)* | **Margin nhỏ thứ hai** (`uxTaskGetSystemState` + 5×HWM + chia 64-bit) |

  - *Lưu ý đối chiếu W4.2*: Mọi task đều phải duy trì `HWM_meas >= 23 words` (tránh tràn stack), đồng thời sai lệch giữa HWM dự đoán ở bảng trên và HWM đo thực tế (`|HWM_pred - HWM_meas|`) mục tiêu nằm trong `±23 words` (`W4E2_STACK_MARGIN_TOLERANCE_WORDS`).

- **Dự đoán CPU load tổng khi hệ thống chạy ổn định (không fault)**: **`20,24%`** *(gồm `18,14%` của 5 task ở LO-Mode + `2,10%` nền ISR `SysTick` W1.3 và `TIM2` W3.1 = `21 µs / 1.000 µs`; khoảng thực đo dự kiến **`16% – 22%`** vì khi `shell` không nhận lệnh liên tục tải danh định khoảng `19,40%` và trình biên dịch `-O2` có thể rút gọn phần PID/convert)*, **thấp hơn nhiều so với trần `W4E1_CPU_BUDGET_PERCENT_MAX = 76%`** (dư biên **`55,76%`** ở LO-Mode và vẫn chỉ **`31,29% <= 76%`** ở HI-Stress worst-case).

---

## 2. Dự đoán hành vi tickless

- **Dự đoán hệ thống có vào STOP mode được không khi `imu`/`telemetry`/`shell` đều rảnh**:
  - Cần phân tách đúng 2 trạng thái theo cơ chế **Adaptive Sleep Gating** đã chốt trong W4.1 (`week4/ex1/REPORT.md` §1.2):
    1. **Khi vòng `control` 1 kHz (`TIM3`) đang hoạt động bình thường**: **KHÔNG vào `STOP mode`**, mà chỉ vào **`SLEEP mode` (`WFI`, `SLEEPDEEP = 0`)** trong khoảng rảnh giữa các nhịp 1 ms. Dù FreeRTOS tính ra `xExpectedIdleTime >= 5 ticks` (do `control` và `imu` chờ `ulTaskNotifyTake(..., portMAX_DELAY)` không nằm trong tick delay list, chỉ còn `telemetry` 10 ms / `idle_aux` 100 ms), hàm `portSUPPRESS_TICKS_AND_SLEEP` phải kiểm tra trạng thái `TIM3->CR1.CEN`, `I2C1` và `DMA1_Ch4`: khi `TIM3` còn chạy, bắt buộc giữ `PLL 72 MHz` và APB clock sống để không cắt nhịp 1 kHz.
    2. **Khi hệ thống rảnh dài hạn thực sự (`expected_idle_ticks >= 5` theo `configEXPECTED_IDLE_TIME_BEFORE_SLEEP = 5`, `TIM3` đã tạm ngưng có chủ đích qua chế độ idle/lệnh shell, `I2C1`/`DMA1_Ch4` rảnh và không ở `HI-Mode`)**: **CÓ vào `STOP mode` (`SLEEPDEEP = 1`, `PWR_CR_LPDS = 1`, `PDDS = 0`)** và đánh thức bằng **`RTC Alarm` trên `EXTI17` dùng nguồn `LSI ≈ 40,11 kHz` đã hiệu chuẩn (W3.2)**, sau đó khôi phục `HSE/PLL 72 MHz` và bù tick chính xác bằng `vTaskStepTick()`.

- **Dự đoán độ trễ thức dậy từ STOP mode có ảnh hưởng tới jitter control loop không, bao nhiêu µs**:
  - **Nếu để vào `STOP mode` giữa các nhịp 1 kHz**: độ trễ thoát `STOP mode` + khóa lại `HSE/PLL 72 MHz` (dữ liệu W3.2) là **`1.710 – 2.210 µs` (1,71 – 2,21 ms)** — gấp **15× – 20×** trần jitter cứng `W4E1_CONTROL_LOOP_JITTER_MAX_US = 113 µs` và lớn hơn cả chu kỳ `1.000 µs` của `control`, chắc chắn phá vỡ deadline ngay lập tức.
  - **Với Adaptive Sleep Gating trong thiết kế W4.1/W4.2**: **KHÔNG ảnh hưởng tới trần jitter `113 µs`** vì:
    - Trong toàn bộ thời gian `control` 1 kHz hoạt động, hệ thống chỉ dùng **`SLEEP mode` (`WFI`)**: độ trễ đánh thức từ `WFI` sang `TIM3_IRQHandler` (NVIC 5) chỉ **`< 2 µs`** (~12–16 chu kỳ ở 72 MHz), giữ trọn `R_LO(control) = 70 µs` và `R_HI(control) = 97 µs <= 113 µs` (biên dư `16 µs`).
    - Khi rảnh dài hạn hạ xuống `STOP mode`, `RTC Alarm` đặt sớm `WAKEUP_OVERHEAD_TICKS = 3 ticks` (3.000 µs > 2.210 µs) nên `HSE/PLL 72 MHz` ổn định xong trước mốc đánh thức lịch FreeRTOS; khi kích hoạt lại vòng 1 kHz, `TIM3->CNT` được đồng bộ lại lệch pha `+500 µs` so với `SysTick` trước khi bật `CEN` nên không tạo jitter hay overrun giả.

---

## 3. Rủi ro đã lường trước

- **Nếu priority đã duyệt ở W4.1 (`control=8 > imu=7 > telemetry=5 > shell=3 > idle_aux=2`) bị vi phạm khi hiện thực**:
  - *Sai thứ tự `imu (8) > control (7)`*: `R_control` tăng từ `70 / 97 µs` lên **`235 µs` (LO)** và **`429 µs` (HI khi recovery I2C1)** -> vượt trần `113 µs` tới `+122 / +316 µs`; quan sát thấy jitter cực đại trên `DWT` vượt ngưỡng cảnh báo `100 µs` và xung `PA4` (`control`) bị đẩy lệch pha theo nhịp `500 Hz` của `imu`.
  - *Đặt `telemetry (8) > control (7)`*: mỗi `10 ms` khi `telemetry` tính CRC32 + nạp `DMA1_Ch4`, `R_control` tăng lên **`277 µs > 113 µs`** -> xuất hiện gai jitter tuần hoàn `100 Hz`.
  - *Đặt `shell (8) > control (7)`*: xảy ra **Criticality Inversion** — `R_control >= 517 µs` khi xử lý lệnh, và khi `UART_FLOOD` thì `control` bị chặn kéo dài làm **`control_overrun_count` tăng liên tục** (bỏ lỡ chu kỳ `1.000 µs`).
  - *Cấu hình nhầm `configMAX_PRIORITIES < 9` (ví dụ copy `3` từ W3.1/W3.2)*: mọi task ưu tiên `>= 3` bị kẹp về mức `2`, mất hoàn toàn ưu tiên phân tầng HI/LO -> `control` bị chen ngang bởi cả `telemetry` và `shell`.
  - *Triệu chứng chung quan sát được*: DWT ghi nhận jitter `control > 100 µs` (ngưỡng báo động sớm) và `> 113 µs` (vỡ ngân sách), `control_overrun_count > 0` nếu trễ vượt `1.000 µs`, và hệ thống kích hoạt cờ `W4E3_BIT_DEGRADED_MODE_ENTERED` / chuyển `HI-Mode`.

- **Nếu `imu` không kịp cung cấp mẫu mới trước khi `control` cần**:
  - **Không block `control` và không bỏ chu kỳ 1 kHz**: `imu` (500 Hz) và `control` (1 kHz) giao tiếp qua **SPSC snapshot ring lock-free** dùng `__DMB()` (W2.2, thời gian đọc `< 1 µs`, blocking `B = 0`), tuyệt đối không dùng `I2C1 Mutex` (nếu khóa chờ bus 400 kHz `433 µs` sẽ đẩy `R_control = 530 µs > 113 µs`).
  - **Phân biệt mẫu dùng lại bình thường vs mẫu quá hạn (`W4E1_DESIGN_IMU_STALE_TICKS = 3 ticks`)**:
    - Bình thường (`imu` 500 Hz, `control` 1 kHz), `control` đọc mẫu gần nhất có tuổi `xTaskGetTickCount() - sample.sample_tick <= 3 ticks` -> vẫn là mẫu hợp lệ, `imu_stale_count` **không tăng**.
    - Khi `imu` trễ mẫu (lỗi `I2C1_ER` `AF/BERR/ARLO`, `ulTaskNotifyTake` timeout `28 ms`, kẹt SDA hoặc `SENSOR_UNPLUG`) làm tuổi mẫu **`> 3 ticks`** (hoặc chưa có mẫu hợp lệ nào sau boot):
      1. `control` tăng **`status.imu_stale_count`** thêm 1 mỗi chu kỳ không có mẫu mới.
      2. `control` chạy **nhánh safe-hold** (`+11 µs`, vẫn nằm trọn trong `C_HI(control) = 97 µs <= 113 µs`): giữ thái độ/lệnh đầu ra ở trạng thái giữ an toàn trung tính (ngừng tích phân PID để tránh integral windup) và hoàn tất đúng chu kỳ (`control_overrun_count = 0`).
      3. Hệ thống đánh dấu suy giảm có kiểm soát (`W4E3_BIT_DEGRADED_MODE_ENTERED`), đồng thời `imu` (P7) thực hiện phục hồi tối đa 9 xung SCL trong ngữ cảnh task (mở ngắt) để `control` (P8) luôn preempt tức thời cho đến khi cảm biến đọc lại được mẫu mới.
