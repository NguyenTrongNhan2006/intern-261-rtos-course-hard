# W4.3 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

- **MSSV**: `2452882` | **MCU**: `STM32F103C8T6` (`Cortex-M3 @ 72 MHz`, `20 KiB SRAM`) | **Ngày chốt**: `21/11/2026` *(commit trước khi viết bất kỳ file `.c` nào của W4.3)*
- **Hồ sơ đối chiếu (SSOT)**:
  - Thiết kế gốc W4.1: `week4/ex1/include/mixed_criticality_design.h`, `week4/ex1/REPORT.md`
  - Số đo nền thực nghiệm W4.2: `week4/ex2/REPORT.md`, `week4/ex2/measurements/w4e2_capture.csv`, `week4/ex2/measurements/w4e2_capture_1mbps.csv`
  - Đặc tả & Tham số W4.3: `week4/ex3/include/params.h` (`W4E3_WCRT_TOLERANCE_PERCENT = 12%`, `W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS = 296 ms`, `W4E3_ASSIGNED_FAULT_NAMES = {"SENSOR_UNPLUG", "TASK_HANG", "CPU_OVERLOAD", "UART_FLOOD"}`), `week4/ex3/include/fault_injection.h`

---

## 1. Dự đoán WCRT/CPU/stack (chạy ổn định 10 phút, chưa fault)

### 1.1. Bảng đối chiếu hai mốc (Mốc A: RTA bảo thủ W4.1 vs Mốc B: Baseline thực đo W4.2 -> Dự đoán 10 phút W4.3)

| Vai trò (`w4e1_task_role_t`) | Prio | T / D (µs) | Mốc A — W4.1 `C_LO / R_LO` (µs) | Mốc A — W4.1 `C_HI / R_HI` (µs) | Mốc B — W4.2 đã đo (`WCRT / HWM / RT%`) | Dự đoán WCRT W4.3 (`10 phút` ổn định, µs) | Độ lệch dự đoán so với `R_LO` W4.1 (%) | Độ lệch dự đoán so với Baseline W4.2 (%) | Dự đoán HWM W4.3 (words / Budget) |
|---|---:|---:|---:|---:|---|---:|---:|---:|---:|
| `W4E1_ROLE_CONTROL` (`control`) | **8** | `1.000 / 1.000` *(Jitter <= 113)* | `70 / 70` | `97 / 97` | `WCRT = 46,97 µs` (`jitter = 29,61 µs`), `HWM = 149 w`, `RT ≈ 3%` | **`47 – 52`** *(Active + ISR latency)* | `-25,7%` đến `-32,9%` *(thấp hơn `R_LO` theo hướng an toàn)* | **`+0,1%` đến `+10,7%` (`<= 12%`)** | **`145 – 149 / 192`** *(dùng `43–47 w`)* |
| `W4E1_ROLE_IMU` (`imu`) | **7** | `2.000 / 2.000` | `165 / 235` | `332 / 429` *(E2E: `862`)* | `HWM = 118 w`, `RT ≈ 3%` (`~60 µs` CPU avg + `~433 µs` chờ bus `400 kHz`) | **Active: `210 – 255`** *(E2E kèm chờ bus: `510 – 570`)* | **Active vs `R_LO`: `-10,6%` đến `+8,5%` (`<= 12%`)** *(E2E vs `668/862`: `-15%` đến `-34%`)* | Bám sát tải CPU `~3%` của W4.2 (`<= 12%`) | **`114 – 118 / 192`** *(dùng `74–78 w`, **margin nhỏ nhất**)* |
| `W4E1_ROLE_TELEMETRY` (`telemetry`) | **5** | `10.000 / 10.000` | `180 / 415` | `180 / 609` | `HWM = 142 w`, `RT < 1%` (`CRC32` 16-entry + `DMA1_Ch4` `80 B`) | **Lệch pha: `90 – 140`** *(Trùng pha `ctl+imu`: **`250 – 320`**)* | `-22,9%` đến `-66,3%` *(thấp hơn `R_LO` do `CRC32 -O2` gọn)* | Khớp `< 1%` CPU của W4.2 | **`138 – 142 / 192`** *(dùng `50–54 w`)* |
| `W4E1_ROLE_SHELL` (`shell`) | **3** | `50.000 / 200.000` | `420 / 835` | `420 / 1.126` | `HWM = 194 w`, `RT ≈ 0%` (parser tĩnh, không `snprintf`) | **Khi thực thi lệnh: `260 – 480`** *(Rảnh timeout: `< 20`)* | `-42,5%` đến `-68,9%` *(thấp hơn `R_LO` do bỏ `snprintf`)* | Khớp `0%` CPU nền của W4.2 | **`180 – 194 / 256`** *(dùng `62–76 w`)* |
| `W4E1_ROLE_IDLE_AUX` (`idle_aux`) | **2** | `100.000 / 100.000` | `250 / 1.155` | `250 / 1.376` | `HWM = 135 w`, `RT < 1%` (`TIM2` stats + `5×HWM`) | **Lệch pha: `180 – 320`** *(Trùng pha preemption + `vTaskGetRunTimeStats`: **`980 – 1.120`**)* | **Đỉnh trùng pha vs `R_LO`: `-3,0%` đến `-15,2%` *(sát biên `12%` nhất)*** | Tăng nhẹ do gọi thêm formatter tĩnh `vTaskGetRunTimeStats` | **`122 – 135 / 192`** *(dùng `57–70 w`)* |

- **Dự đoán task nào lệch WCRT đo được so với RTA ở W4.1 nhiều nhất, trong ngưỡng `W4E3_WCRT_TOLERANCE_PERCENT = 12%`**:
  1. **Xét trong nhóm bám sát công thức RTA của W4.1 trong biên `±12%`**:
     - **`idle_aux` (P2)** (khi đo đỉnh trùng pha bị cả 4 task `control + imu + telemetry + shell` preempt kèm lượt gọi `vTaskGetRunTimeStats` + `5 × uxTaskGetStackHighWaterMark`: dự đoán **`1.020 – 1.120 µs`** so với `R_LO = 1.155 µs`, lệch **`-3,0%` đến `-11,7% <= 12%`**) và **`imu` (P7)** (WCRT hoạt động `C_imu + C_control` dự đoán **`210 – 255 µs`** so với `R_LO = 235 µs`, lệch **`-10,6%` đến `+8,5% <= 12%`**). Trong hai vai trò này, **`idle_aux`** lệch sát ranh giới `12%` nhất vì nằm ở mức ưu tiên thấp nhất (`P2`), cộng dồn toàn bộ phương sai preemption của 4 tầng phía trên.
  2. **Giải thích trung thực vì sao `control`, `telemetry`, `shell` có WCRT thực đo thấp hơn W4.1 vượt ngoài `-12%` (lệch về phía AN TOÀN `WCRT_meas < R_W4.1`)**:
     - Ở W4.1, bảng `W4E1_RTA_TABLE` được thiết kế là **cận trên Worst-Case bảo thủ (Analytical Upper Bound)** trước khi viết code W4.2:
       - Với **`control`**: W4.1 dự trù `C_LO = 70 µs` (`C_HI = 97 µs`) cho bộ lọc bổ sung + PID 3 trục, trong khi hiện thực W4.2 ở `-O2` dùng luật PID 1 trục gọn (`accel_g[0]`, `gyro_dps[1]`, `clamp_control`, không gọi `libm`) nên WCRT thực tế trên board qua `> 25,9 phút` ở W4.2 chỉ hết **`46,97 µs`** (`-32,9%` so với `R_LO = 70 µs` và `-51,6%` so với `R_HI = 97 µs`). So với **Mốc B (Baseline W4.2 = `46,97 µs`)**, WCRT `control` trong 10 phút W4.3 (`47 – 52 µs`) sai lệch chỉ **`+0,1%` đến `+10,7%` (`<= 12%`)**.
       - Với **`telemetry` và `shell`**: W4.1 ước lượng `CRC32` theo tốc độ `-O0` (`110 cycles/B`) và dự trù chi phí format chuỗi nặng, trong khi W4.2 dùng bảng `CRC32` 16 phần tử ở `-O2` và `reply_writer_t` tĩnh (`0` `snprintf`), đồng thời `TIM3` đặt lệch pha `+500 µs` so với `SysTick` nên trong vận hành ổn định không xảy ra **Critical Instant** cả 5 task phát hành cùng một chu kỳ xung nhịp.
     - Theo quy tắc SSOT, tuyệt đối giữ nguyên bảng `W4E1_RTA_TABLE` để đối chiếu minh bạch cả hai mốc (so với cận trên W4.1 và so với baseline W4.2).

- **Dự đoán CPU load tổng đo được (`10 phút` chạy ổn định, `TIM2` `100 kHz` / `vTaskGetRunTimeStats`)**: **`7,0% – 9,0%`** *(điểm danh định **`~8,0%`**, khớp tuyệt đối với baseline W4.2 là `7,13% – 8,00%`, gồm `control ~3%`, `imu ~3%`, các task LO + nền ISR `SysTick`/`TIM2` `~1% – 3%`)* — thấp hơn **`2,5×`** so với dự đoán RTA bảo thủ W4.1 (`20,24%` LO-Mode / `31,29%` HI-Stress) và thấp hơn gần **`10×`** so với trần ngân sách `W4E1_CPU_BUDGET_PERCENT_MAX = 76%` (dư biên `~68%`).

- **Dự đoán Stack High-Water Mark (`uxTaskGetStackHighWaterMark`) đo định kỳ trong 10 phút**:
  - Giữ ổn định quanh mốc W4.2 (`control: 145–149 w`, `imu: 114–118 w`, `telemetry: 138–142 w`, `shell: 180–194 w`, `idle_aux: 122–135 w`).
  - **`imu`** tiếp tục là task có HWM nhỏ nhất (`~114–118 / 192 words`), mọi task đều duy trì biên dư **`>= 114 words >> 23 words`** (`W4E2_STACK_MARGIN_TOLERANCE_WORDS`), và sau khi chạy qua toàn bộ các nhánh lệnh, độ trôi HWM theo thời gian trong 10 phút bằng **`0 words`** (không rò rỉ stack).

---

## 2. Dự đoán từng kịch bản fault (`W4E3_ASSIGNED_FAULT_NAMES` trong `PARAMS.md`)

### Bảng Tóm tắt Dự đoán 4 Kịch bản Fault được giao (MSSV `2452882`)

| Thứ tự (`PARAMS.md`) | Tên kịch bản & Enum (`w4e3_fault_kind_t`) | Cách kích hoạt dự kiến | Cơ chế phát hiện & Bit đồng bộ (`xEventGroupWaitBits`) | `control` (1 kHz) có trễ deadline (`1.000 µs`) hay vượt jitter (`113 µs`) không? | Có phải suy giảm có kiểm soát đã công bố ở W4.1 (`was_declared_controlled_degradation`)? | Dự đoán `watchdog_reset_reason_code` (`IWDG <= 296 ms` + `.noinit`) |
|---:|---|---|---|---|---|---|
| **1** | **`SENSOR_UNPLUG`** (`W4E3_FAULT_SENSOR_UNPLUG`) | Rút dây `I2C1` (`PB6/PB7`) của `MPU6050` hoặc lệnh shell `fault sensor_unplug` / `fault_injection_trigger()` | `I2C1_ER` (`AF/BERR/ARLO`) hoặc timeout `28 ms`; tuổi mẫu SPSC `> 3 ticks` -> set **`W4E3_BIT_DEGRADED_MODE_ENTERED`** | **KHÔNG** (`control_deadline_missed = false`, `overrun = 0`, `R <= 97 µs <= 113 µs`) | **CÓ (`true`)** — Mục 1 `W4E1_FAILURE_MODE_TABLE` (`control` chạy safe-hold, `imu` recovery 9 xung SCL ở P7 mở ngắt) | **`0`** *(Không reset; hệ thống tự duy trì ở chế độ safe-hold và tự hồi phục khi cắm lại cảm biến)* |
| **2** | **`TASK_HANG`** (`W4E3_FAULT_TASK_HANG`) | Lệnh shell `fault task_hang` hoặc `fault_injection_trigger()`: treo vô hạn 1 task (2A: treo task LO P3/P5 theo `README.md`; 2B: treo task HI P8) | Thiếu bit trong `HEARTBEAT_MASK` sau cửa sổ `100 ms` (hoặc task treo làm starve `idle_aux` P2 không kick `IWDG`) -> sau reboot set **`W4E3_BIT_WATCHDOG_RECOVERED`** & **`W4E3_BIT_DEGRADED_MODE_ENTERED`** | **2A (treo task Prio `< 8`): KHÔNG** (`false`, `control` P8 vẫn preempt bình thường cho tới lúc IWDG reset). **2B (treo chính `control` P8): CÓ** (`true`, dừng vòng lặp cho tới lúc IWDG reset `<= 296 ms`) | **CÓ (`true`)** — Mục 4 `W4E1_FAILURE_MODE_TABLE` (ghi role treo, `PC/LR/CFSR`, canary `0x6C217697` vào `.noinit` và reset `IWDG <= 296 ms`) | **`!= 0`** *(Mã reset `TASK_HANG` kèm ID role bị treo được bảo toàn qua reset trong `.noinit` và xác nhận bởi `RCC_CSR_IWDGRSTF`)* |
| **3** | **`CPU_OVERLOAD`** (`W4E3_FAULT_CPU_OVERLOAD`) | Lệnh shell `fault cpu_overload` hoặc `fault_injection_trigger()`: bơm tải busy-wait (3A: tải ở Prio `< 8` vượt `CPU > 76%`; 3B: task giả ở Prio `>= 8`) | **3A**: `TIM2` báo `CPU > 76%` hoặc DWT `jitter > 100 µs` -> vào `HI-Mode`, shed `telemetry`+`shell`, set **`W4E3_BIT_DEGRADED_MODE_ENTERED`**. **3B**: `TIM3` thấy `control` bị starve -> `.noinit` + `IWDG <= 296 ms` | **3A (quá tải ở Prio `< 8`): KHÔNG** (`false`, shed LO giữ `R_control <= 97 µs`). **3B (task busy-wait ở Prio `>= 8`): CÓ** (`true`, chiếm 100% CPU chặn P8 cho tới khi IWDG cắt trong `<= 296 ms`) | **CÓ (`true`) cho cả 3A & 3B** — Mục 3 `W4E1_FAILURE_MODE_TABLE` và `W4.1 REPORT.md` §2.3 đã công bố rõ: *"0 trễ deadline (trừ task giả Prio >= 8 -> IWDG xử lý)"* | **3A: `0`** *(HI-Mode shedding bảo vệ sống `control` không cần reset)*. **3B: `!= 0`** *(IWDG reset `<= 296 ms`, ghi mã `CPU_OVERLOAD` vào `.noinit`)* |
| **4** | **`UART_FLOOD`** (`W4E3_FAULT_UART_FLOOD`) | Bơm liên tục `1 Mbps` vào `USART1 RX` nhanh hơn `200 ms` (`> 256 B / 50 ms`) từ script host hoặc lệnh `fault uart_flood` | `StreamBuffer` drop (`sent < batch`), cờ `ORE/FE/NE`, hoặc `rx_bytes > 256` -> mask cả `USART1 IDLEIE` & `DMA1_Ch5 HTIE/TCIE` trong `50 ms`, set **`W4E3_BIT_DEGRADED_MODE_ENTERED`** | **KHÔNG** (`control_deadline_missed = false`, `overrun = 0`, `R <= 97 µs <= 113 µs`) | **CÓ (`true`)** — Mục 2 `W4E1_FAILURE_MODE_TABLE` (cắt bão ngắt `1.562,5 Hz` trong `50 ms` cooldown, xả khung rác, hạ `telemetry` về `10 Hz`) | **`0`** *(Không reset; `shell` ngủ cooldown `5 ms/vòng` vẫn giữ heartbeat, `control` và `imu` hoàn toàn miễn nhiễm)* |

### Phân tích chi tiết từng kịch bản

- **Kịch bản 1 (`SENSOR_UNPLUG` — Ngắt kết nối `I2C1` `SDA/SCL` của `MPU6050`)**:
  - **Control loop có trễ deadline không**: **KHÔNG** (`control_deadline_missed = false`).
  - **Có phải suy giảm đã công bố ở W4.1 không**: **CÓ** (`was_declared_controlled_degradation = true`, `W4E1_FAILURE_MODE_TABLE[0]`).
  - **Giải thích cơ chế & định lượng**: `control_task` (P8) không bao giờ gọi trực tiếp driver `I2C1` mà chỉ đọc mẫu mới nhất từ `imu_ring` qua cơ chế **SPSC lock-free** (`atomic_load_explicit / memory_order_acquire`, tốn `< 1 µs`, blocking `B = 0`). Khi rút dây `SDA/SCL`, `I2C1_ER_IRQHandler` (NVIC 6) chỉ mất `< 6 µs` để tắt ngắt I2C và đánh thức `imu_task` (P7) thực hiện `recover_bus()` (9 xung SCL `~90–120 µs` với **ngắt mở**), sau đó lùi chu kỳ nghỉ `20 ms`. Sau `3 ticks` (`3 ms`), `control_task` thấy `xTaskGetTickCount() - sample.sample_tick > W4E1_DESIGN_IMU_STALE_TICKS`, lập tức chuyển sang nhánh **safe-hold** (`control_output = 0.0f`, tăng `status.imu_stale_count`) và dựng cờ `W4E3_BIT_DEGRADED_MODE_ENTERED`. Thực nghiệm ở W4.2 (`31` lần recovery I2C1 liên tiếp) đã xác nhận `overrun = 0` và `jitter_max = 22,61 µs <= 113 µs`.

- **Kịch bản 2 (`TASK_HANG` — Treo vô hạn 1 task)**:
  - **Control loop có trễ deadline không**:
    - **Với kịch bản chuẩn `README.md` (treo task ưu tiên thấp P3 `shell` hoặc P5 `telemetry` bằng vòng lặp busy-wait vô hạn)**: **KHÔNG** (`control_deadline_missed = false`). Vì `TIM3_IRQn` (NVIC 5) và `control_task` (P8) có ưu tiên tuyệt đối cao hơn P3/P5, vòng điều khiển 1 kHz vẫn thực thi đúng hạn mỗi `1.000 µs` trong suốt khoảng thời gian chờ Watchdog cắt (`<= 296 ms`).
    - **Nếu mentor yêu cầu tiêm treo trực tiếp vào chính `control_task` (P8)**: **CÓ** (`control_deadline_missed = true`), vì bản thân task điều khiển bị khóa cứng.
  - **Có phải suy giảm đã công bố ở W4.1 không**: **CÓ** (`was_declared_controlled_degradation = true`, `W4E1_FAILURE_MODE_TABLE[3]`).
  - **Giải thích cơ chế & phục hồi Watchdog**: Khi một task bị treo, nó ngừng cập nhật bit của mình trong `heartbeat` EventGroup; hơn nữa nếu nó busy-wait ở P3/P5 thì nó còn chặn `idle_aux_task` (P2) không cho chạy để nạp `IWDG`. Bản ghi `.noinit` (canary `0x6C217697`, checksum FNV-1a) ghi nhận mã lỗi `TASK_HANG` + ID role treo + `PC/LR`, và `IWDG` (prescaler `/8`, reload `1109`) cưỡng bức reset phần cứng trong **`<= 296 ms`** (`~221 ms` ở `LSI = 40,11 kHz`). Ngay khi boot lại, `fault_injection_init()` đọc `.noinit` + cờ `RCC_CSR_IWDGRSTF`, điền `watchdog_reset_reason_code != 0` và dựng `W4E3_BIT_WATCHDOG_RECOVERED | W4E3_BIT_DEGRADED_MODE_ENTERED`.

- **Kịch bản 3 (`CPU_OVERLOAD` — Quá tải CPU)**:
  - **Phân tách rõ 2 mức ưu tiên của nguồn gây quá tải**:
    1. **Mức 3A — Quá tải ở tầng LO / trung gian (`Priority < 8`, làm tổng CPU vượt trần `W4E1_CPU_BUDGET_PERCENT_MAX = 76%` hoặc jitter `control > 100 µs`)**:
       - **Control loop có trễ deadline không**: **KHÔNG** (`control_deadline_missed = false`).
       - **Có phải suy giảm đã công bố ở W4.1 không**: **CÓ** (`was_declared_controlled_degradation = true`, `W4E1_FAILURE_MODE_TABLE[2]`).
       - **Hành vi**: Hệ thống chuyển từ `LO-Mode` sang **`HI-Mode`**, dựng cờ `W4E3_BIT_DEGRADED_MODE_ENTERED`, đình chỉ (`vTaskSuspend`) các tác vụ LO-criticality (`telemetry` P5, `shell` P3 và nguồn tải LO), bỏ các bit LO khỏi mặt nạ heartbeat kỳ vọng, và khóa `STOP mode` ở `idle_aux` để giữ `PLL 72 MHz`. Vòng `control` (P8) và `imu` (P7) giữ trọn `0` deadline miss (`watchdog_reset_reason_code = 0`).
    2. **Mức 3B — Tiêm 1 task busy-wait vô hạn ở `Priority >= 8` (ngang hoặc cao hơn `control` P8, theo gợi ý trong `README.md` mục 4)**:
       - **Control loop có trễ deadline không**: **CÓ** (`control_deadline_missed = true`), vì trong lập lịch ưu tiên cố định không chia lát thời gian (`configUSE_TIME_SLICING = 0`), một task busy-wait không block ở `Priority >= 8` chiếm 100% CPU và chặn đứng `control_task` (P8) lẫn `idle_aux_task` (P2).
       - **Có phải suy giảm đã công bố ở W4.1 không**: **CÓ** (`was_declared_controlled_degradation = true`). Trong `week4/ex1/REPORT.md` §2.3 (dòng 214) đã công bố tường minh từ giai đoạn thiết kế: **`0 trễ deadline` *(trừ task giả Prio >= 8 -> IWDG xử lý)***.
       - **Hành vi**: `TIM3_IRQHandler` (NVIC 5, chạy mỗi `1 ms` ngay cả khi task P8/P9 chiếm CPU) phát hiện `missed_releases > 0` (`control` bị starve), lập tức cập nhật bản ghi `.noinit` với mã lỗi `CPU_OVERLOAD` + dựng `W4E3_BIT_DEGRADED_MODE_ENTERED`, và cắt task quá tải (hoặc để `IWDG` nổ reset cứng trong **`<= 296 ms`** khi task quá tải khóa chết CPU), bảo đảm khôi phục hệ thống kèm đầy đủ bằng chứng log `.noinit`.

- **Kịch bản 4 (`UART_FLOOD` — Bơm ngập `USART1 RX` ở tốc độ dây `1 Mbps`)**:
  - **Control loop có trễ deadline không**: **KHÔNG** (`control_deadline_missed = false`).
  - **Có phải suy giảm đã công bố ở W4.1 không**: **CÓ** (`was_declared_controlled_degradation = true`, `W4E1_FAILURE_MODE_TABLE[1]`).
  - **Giải thích cơ chế & định lượng**: Khi host flood liên tục ở `1 Mbps` (`100.000 B/s`), nửa vòng `DMA1_Ch5` (`64 B`) đầy mỗi `640 µs` (`1.562,5 Hz`). Ngay khi `StreamBuffer` (`511 B`) đầy (`xStreamBufferSendFromISR < batch`), hoặc xuất hiện cờ `ORE/FE/NE`, hoặc `shell_task` nhận `> 256 B / 50 ms` hay quá `200 ms` (`W4E2_SHELL_CMD_TIMEOUT_MS`), hàm `begin_cooldown()` tắt đồng thời cả **`USART1 IDLEIE/RE` lẫn `DMA1_Ch5 CCR (HTIE/TCIE)`** trong **`50 ms`**, dựng cờ `W4E3_BIT_DEGRADED_MODE_ENTERED`, xả bỏ khung lỗi và giảm nhịp `telemetry` từ `100 Hz` xuống `10 Hz`. Nhờ mỗi ISR trước khi khóa chỉ tốn `<= 13,7 µs` ở NVIC `7` (`< TIM3` NVIC `5`) và bão ngắt bị cắt đứt hoàn toàn trong `50 ms` cooldown, `control_task` (P8) không mất một chu kỳ nào (`overrun = 0`, `jitter <= 113 µs`).

---

## 3. Rủi ro đã lường trước

### 3.1. Nếu Watchdog (`IWDG`) không phục hồi đúng `W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS = 296 ms`
1. **Rủi ro trễ hạn `> 296 ms` do dung sai tần số `LSI` (`30 – 60 kHz` trên STM32F103)**:
   - Bộ đếm `IWDG` chạy bằng dao động nội `LSI` danh định `40 kHz` nhưng datasheet quy định dải thực tế từ **`30 kHz` đến `60 kHz`**. Nếu tính `RLR` theo `40 kHz` để đặt timeout sát `296 ms` (ví dụ prescaler `/8`, `RLR = 1479`), thì khi gặp chip có `LSI = 30 kHz`, thời gian reset thực tế bị kéo dài tới `(1479 + 1) × 8 / 30.000 = 394,7 ms > 296 ms` (vượt trần `+98,7 ms`).
   - **Hệ quả quan sát được**: Khi xảy ra `TASK_HANG` hoặc `CPU_OVERLOAD` (Prio `>= 8`), hệ thống bị treo lâu hơn ngưỡng cho phép gần `100 ms`; đầu ra điều khiển (`control_output`) bị giữ cứng ở trạng thái cũ không an toàn thêm hàng trăm chu kỳ `1 kHz`, và phép đo thời gian phục hồi từ lúc tiêm lỗi đến lúc MCU reboot báo **FAIL** (`> 296 ms`).
   - **Biện pháp khóa thiết kế**: Cấu hình `IWDG` với **Prescaler `/8` (`PR = 1`)** và **`RLR = 1109`** (đã chốt ở W4.1 §5.1):
     - Tại biên chậm nhất `LSI = 30 kHz`: `T_wdg = 1110 × 8 / 30.000 = 296,0 ms <= 296 ms` (**không bao giờ vượt trần**).
     - Tại tần số thực tế của board đo ở W3.2 (`LSI ≈ 40,11 kHz`): `T_wdg ≈ 221,4 ms <= 296 ms`.
     - Tại biên nhanh nhất `LSI = 60 kHz`: `T_wdg = 1110 × 8 / 60.000 = 148,0 ms > 100 ms` (lớn hơn chu kỳ kiểm tra heartbeat `100 ms` của `idle_aux`, dư `48 ms` để không gây reset giả).
2. **Rủi ro Watchdog không bao giờ nổ khi task treo ("Blind Kick") hoặc mất bản ghi `.noinit`**:
   - Nếu đặt lệnh nạp `IWDG->KR = 0xAAAA` trong `TIM3_IRQHandler` / `SysTick_Handler`, hoặc `idle_aux` nạp `IWDG` mà không kiểm tra đủ mặt nạ `HEARTBEAT_MASK` của mọi task đang active, thì khi một task bị treo hệ thống sẽ **không bao giờ reset**, làm `fault_injection_trigger(W4E3_FAULT_TASK_HANG)` bị kẹt vĩnh viễn.
   - Ngược lại, khi một task ở P3..P8 busy-wait 100% CPU làm `idle_aux` (P2) bị starve hoàn toàn, `IWDG` sẽ nổ reset phần cứng bất đồng bộ mà **không chạy qua `idle_aux` hay `HardFault_Handler`**. Nếu chỉ đợi `idle_aux` phát hiện lỗi mới ghi `.noinit`, vùng `.noinit` sẽ không có dữ liệu (`watchdog_reset_reason_code = 0` sau reboot).
   - **Biện pháp khóa thiết kế**: (a) Chỉ nạp `IWDG` trong `idle_aux` khi `(observed & expected_active_mask) == expected_active_mask`; (b) **Pre-arm bản ghi `.noinit`** (canary `0x6C217697`, mã kịch bản, role mục tiêu, checksum FNV-1a + `__DSB()`) ngay khi kích hoạt fault và cập nhật từ `TIM3_IRQHandler` (NVIC 5) khi phát hiện `heartbeat` không được thu gom quá `150 ms` hoặc `missed_releases > 0`.
3. **Rủi ro Watchdog nổ giả (False Reset) khi shed task ở `HI-Mode` hoặc khi ngủ `STOP mode`**:
   - Khi vào `HI-Mode` (`CPU_OVERLOAD` / `UART_FLOOD`), `telemetry` và `shell` bị đình chỉ có chủ đích; nếu không gỡ 2 bit này khỏi `expected_active_mask`, `idle_aux` sẽ ngừng nạp `IWDG` và gây reset giả sau `221 ms`.
   - Khi `IWDG` đã bật trên STM32F103, bộ đếm `LSI` của `IWDG` vẫn chạy trong `STOP mode`. Nếu `MAX_STOP_TICKS` để `1.000 ms` như W4.2, MCU ngủ `STOP` quá `221 ms` sẽ bị `IWDG` reset giữa lúc ngủ. Do đó khi bật `IWDG` ở W4.3, thời gian ngủ `STOP` mỗi lượt (`MAX_STOP_TICKS`) phải kẹp trần **`<= 80 ticks` (`80 ms < 148 ms`)** để thức dậy nạp `IWDG` an toàn.

### 3.2. Rủi ro xung đột giữa API bắt buộc `vTaskGetRunTimeStats` (`RUBRIC.md`) và `configSUPPORT_DYNAMIC_ALLOCATION = 0` (`check_api.sh`)
- **Phân tích nguyên nhân gốc rễ trong `third_party/FreeRTOS-Kernel/tasks.c:7383-7564`**:
  - Hàm `vTaskGetRunTimeStatistics()` gốc của FreeRTOS V11.1.0 (được gọi bởi macro `vTaskGetRunTimeStats(pcWriteBuffer)` trong `task.h:2416`) khi bật `configUSE_STATS_FORMATTING_FUNCTIONS = 1` sẽ gọi trực tiếp **`pvPortMalloc(uxCurrentNumberOfTasks * sizeof(TaskStatus_t))`** (dòng `7435`) và **`snprintf()`** (dòng `7492`).
  - Với `configSUPPORT_DYNAMIC_ALLOCATION = 0`, `pvPortMalloc` trong `tasks.c` trả về `NULL`, làm `vTaskGetRunTimeStats()` của kernel **thoát ngay và trả về chuỗi rỗng `""`**, đồng thời kéo `snprintf` của `newlib` vào binary.
- **Giải pháp triệt để cho W4.3**:
  - Giữ `configSUPPORT_DYNAMIC_ALLOCATION = 0` và `configUSE_STATS_FORMATTING_FUNCTIONS = 0` trong `FreeRTOSConfig.h`.
  - Tận dụng việc macro `vTaskGetRunTimeStats(pcWriteBuffer)` trong `task.h:2416` luôn mở rộng thành `vTaskGetRunTimeStatistics((pcWriteBuffer), configSTATS_BUFFER_MAX_LENGTH)`: hiện thực hàm **`vTaskGetRunTimeStatistics(char *pcWriteBuffer, size_t uxBufferLength)`** bằng bộ đệm tĩnh `static TaskStatus_t s_task_status[8]` kết hợp `uxTaskGetSystemState()` và bộ format số nguyên tĩnh (`0` `pvPortMalloc`, `0` `snprintf`), đảm bảo lệnh gọi `vTaskGetRunTimeStats()` hoạt động chính xác 100% với `0` byte heap.
