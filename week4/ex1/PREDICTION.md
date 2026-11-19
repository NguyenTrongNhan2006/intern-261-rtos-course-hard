# W4.1 – PREDICTION (điền TRƯỚC khi nộp design doc cho mentor duyệt)

MSSV: **2452882** | MCU: **STM32F103C8T6** (Cortex-M3 @ 72 MHz, 20 KiB SRAM) | Nguồn tham số: `include/params.h` và `PARAMS.md` (sinh bằng `python tools/gen_params.py 2452882`, bị `.gitignore` nên không có trên GitHub) và [include/mixed_criticality_design.h](include/mixed_criticality_design.h).

**Nhật ký sửa đổi** — mọi lần sửa đều diễn ra **trước** khi mentor duyệt và trước khi có bất kỳ số đo W4.2 nào, nên không phải chỉnh dự đoán cho khớp kết quả:

| Ngày | Thay đổi | Lý do |
|---|---|---|
| 18/11/2026 | Bản đầu; audit: $C_{HI}(control)$ 90 → 97 µs | Các khoản phân rã cộng ra 97 |
| 19/11/2026 | `imu` đọc burst 14 byte, `I2C1` lên 400 kHz: $C(imu)$ 140/260 → 165/332 µs; tính lại RTA, thêm kiểm tra hoàn tất có chờ bus | Mẫu cần cả accel và gyro; ở 100 kHz `imu` trễ deadline |
| 19/11/2026 | Nền ISR 2,11 → 2,10 %; ngưỡng stale > 3 tick; vòng DMA RX 256 → 128 B (RAM 11.904 → 11.776 B); gắn nhãn nguồn cho từng $C_i$ | Sai số học; báo stale sai; ISR UART chép 128 B vượt 16 µs đã dành |

Quy ước nhãn nguồn: **Đo** = có số đo trong repo (ghi rõ bài); **Suy luận** = tính ra từ số đo hoặc ngân sách đã kiểm của tuần trước; **Ước lượng** = chưa có số đo, W4.2 phải đo bằng DWT và so với sai số cho phép `W4E3_WCRT_TOLERANCE_PERCENT` = 12 %.

---

## Dự đoán RTA

- Bảng $T / D / C / R$ cho 5 vai trò (khớp nguyên văn mảng `W4E1_RTA_TABLE` trong [include/mixed_criticality_design.h](include/mixed_criticality_design.h)), điền bằng tay TRƯỚC khi mentor duyệt:

| Role | Criticality | $T$ ($\mu\text{s}$) | $D$ ($\mu\text{s}$) | $C_{LO}$ ($\mu\text{s}$) | $C_{HI}$ / `wcet_us` ($\mu\text{s}$) | $R_{LO}$ dự đoán ($\mu\text{s}$) | $R_{HI}$ / `response_time_us` ($\mu\text{s}$) | Margin $D_i - R_{HI}$ ($\mu\text{s}$) | Priority |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `control` | **HI** | 1.000 | 1.000 | 70 | **97** | 70 | **97** | 903 (Jitter margin: **16**) | **8** |
| `imu` | **HI** | 2.000 | 2.000 | 165 | **332** | 235 | **429** | 1.571 (hoàn tất kể cả chờ bus: 862 → **1.138**) | **7** |
| `telemetry` | **LO** | 10.000 | 10.000 | 180 | **180** (Shed $\rightarrow 0$) | 415 | **609** (Shed $\rightarrow$ N/A) | 9.391 | **5** |
| `shell` | **LO** | 50.000 | 200.000 | 420 | **420** (Shed $\rightarrow 0$) | 835 | **1.126** (Shed $\rightarrow$ N/A) | 198.874 | **3** |
| `idle_aux` | **LO** | 100.000 | 100.000 | 250 | **250** (HI $\rightarrow 80$) | 1.155 | **1.376** (HI $\rightarrow 509$) | 98.624 | **2** |

### Nguồn gốc $C_i$ (72 MHz, `-O2`)

**1. `control`** (Priority 8, 1 kHz, đánh thức bởi `TIM3_IRQHandler`)

| Thành phần | µs | Nhãn | Nguồn |
|---|---:|---|---|
| `TIM3_IRQHandler` + `vTaskNotifyGiveFromISR` + chuyển ngữ cảnh | 8 | Suy luận | W1.2: OUT → IN 219–244 cycle (3,0–3,4 µs) ở `-O2`; phần ISR + notify còn lại là ước lượng |
| Đọc mẫu IMU qua SPSC seqlock | 1 | Đo | W2.2: pop 24 cycle ≈ 0,33 µs, làm tròn lên |
| Lọc bổ sung + PID 3 trục | 56 | Ước lượng | Chưa đo |
| Cập nhật `w4e2_system_status_t` + bit heartbeat | 5 | Ước lượng | Chưa đo |
| **$C_{LO}$** | **70** | | |
| Nhánh kiểm tra mẫu cũ + giữ tư thế an toàn (safe-hold) | +11 | Ước lượng | Chưa đo |
| Bị 1 ngắt `USART1` RX chen ngang | +16 | Suy luận | Ngân sách W3.1: 220 cycle/sự kiện + 12 cycle/B; vòng DMA 128 B ⇒ ≤ 64 B/sự kiện ⇒ 988 cycle ≈ 13,7 µs, làm tròn lên |
| **$C_{HI}$** | **97** | | |

**2. `imu`** (Priority 7, 500 Hz, được `TIM3_IRQHandler` đánh thức mỗi 2 tick, cùng pha với `control`)

Mẫu `w4e2_imu_sample_t` cần cả `accel_g[3]` và `gyro_dps[3]`, nên phải đọc burst **14 byte** MPU6050 từ `0x3B` tới `0x48` (accel 6 + temp 2 + gyro 6). W2.1 chỉ đo đọc **6 byte** (`ACCEL`, 7 lần vào ISR, 646,583 µs). Bus `I2C1` chạy **Fast-mode 400 kHz** (MPU6050 hỗ trợ; `CCR = 30` với `F/S = 1`, `DUTY = 0`, `TRISE = 11` ở `PCLK1 = 36 MHz`).

| Thành phần | µs | Nhãn | Nguồn |
|---|---:|---|---|
| Đánh thức task sau notify | 7 | Ước lượng | Chưa đo |
| Quy đổi dữ liệu thô + ghi SPSC | 98 | Ước lượng | Chưa đo |
| 19 lần vào ISR `I2C1_EV/ER` × trung bình 3,18 µs | 60 | Suy luận | W2.1: 4 lần cho pha ghi + 7 lần cho đọc 6 byte, marker 1,000–5,583 µs; đọc 14 byte suy ra 15 lần (`SB`, `ADDR`, 12 × `BTF`, `RXNE`) |
| **$C_{LO}$** | **165** | | |
| Lấy trần 5,583 µs/lần thay trung bình ($19 \times 5{,}583 \approx 107$) | +47 | Suy luận | Trần đo ở W2.1, số lần là suy luận |
| Giải cứu bus khi SDA kẹt, trong ngữ cảnh task: 9 xung SCL ≈ 90 + START/STOP 17,75 + nạp lại `CCR/TRISE` ≈ 12 | +120 | Suy luận | START/STOP đo ở W2.1; nhánh 9 xung **chưa từng chạy** (`PULSES=0` cả 3 log) |
| **$C_{HI}$** | **332** | | |
| *Chờ bus (task Blocked, không thuộc $C$)*: ghi 49,2 + khoảng nghỉ 36,5 + đọc 15 byte 346,4 | *433* | Suy luận | W2.1: 92,37 µs/byte ở 100 kHz, chia 4. Nếu giữ 100 kHz: ≈ 1.619 µs và `imu` trễ deadline |

Lượt `RECOVERED` nhanh ở W2.1 đo được 153–427 µs wall-clock (chưa tách phần CPU). Với cận bảo thủ $C_{HI}(imu) = 729\ \mu s$ tập task vẫn schedulable (REPORT §5.4).

**3. `telemetry`** (Priority 5, 100 Hz) — $C_{LO} = C_{HI} = 180\ \mu s$; ở `HI-Mode` bị cắt tải (0 µs)

| Thành phần | µs | Nhãn | Nguồn |
|---|---:|---|---|
| CRC32 phần mềm, khung 70 B × 110 cycle/B = 7.700 cycle | 107 | Suy luận | Ngân sách W3.1, kiểm gián tiếp: Parser đo 0,78 % / dự đoán 0,80 % (ở 460.800 bps) |
| Đọc snapshot, đóng gói khung, nạp `DMA1_Channel4`, ngắt TC | 73 | Ước lượng | Chưa đo; phát 70 B ở 1 Mbps mất 0,7 ms nhưng do DMA, không tốn CPU |

**4. `shell`** (Priority 3, sporadic $T_{min} = 50$ ms, $D = 200$ ms) — $C = 420\ \mu s$

| Thành phần | µs | Nhãn | Nguồn |
|---|---:|---|---|
| Parser tách khung + CRC32 + `mixed_criticality_system_shell_exec()` | 350 | Ước lượng | Chưa đo |
| ISR chép byte lệnh vào `StreamBuffer` (khung 70 B: 2 sự kiện × 220 + 70 × 12 = 1.280 cycle ≈ 18 µs) | 25 | Suy luận | Ngân sách W3.1, làm tròn lên |
| Định dạng chuỗi phản hồi vào `reply_buf` | 45 | Ước lượng | Chưa đo |

**5. `idle_aux`** (Priority 2, $T = D = 100$ ms) — $C = 250\ \mu s$; ở `HI-Mode` chỉ còn 80 µs

| Thành phần | µs | Nhãn | Nguồn |
|---|---:|---|---|
| Kiểm tra mặt nạ heartbeat, nạp `IWDG` (trước hạn 296 ms), chọn chế độ ngủ | 180 | Ước lượng | Chưa đo |
| `uxTaskGetSystemState()` (bộ đếm `TIM2` 100 kHz của W3.1) + 5 lần `uxTaskGetStackHighWaterMark()` | 50 | Ước lượng | Chưa đo |
| Phép chia 64-bit hiệu chuẩn LSI theo HSE (W3.2) | 20 | Ước lượng | Chưa đo |

Nhận xét: trong 5 vai trò chỉ có vài thành phần là số đo trực tiếp; phần lớn là ước lượng, nên WCET của W4.2 phải đo bằng DWT cho **từng** vai trò trước khi tin vào bảng RTA.

### Từng bước lặp hội tụ RTA bằng tay ($R_i^{(k+1)} = C_i + \sum_{j \in hp(i)} \lceil R_i^{(k)} / T_j \rceil C_j$)
- **`control` ($i=0$)**: $hp(0) = \emptyset \Rightarrow R_0^{(0)} = C_0 = \mathbf{97\text{ }\mu\text{s}} \le D_0 = 1.000\text{ }\mu\text{s}$ (và $\le J_{max} = 113\text{ }\mu\text{s}$).
- **`imu` ($i=1$)**:
  - $R_1^{(0)} = 332\text{ }\mu\text{s}$
  - $R_1^{(1)} = 332 + \lceil 332 / 1000 \rceil \times 97 = 332 + 97 = 429\text{ }\mu\text{s}$
  - $R_1^{(2)} = 332 + \lceil 429 / 1000 \rceil \times 97 = \mathbf{429\text{ }\mu\text{s}} \le D_1 = 2.000\text{ }\mu\text{s}$ (Hội tụ).
- **`telemetry` ($i=2$)**:
  - $R_2^{(0)} = 180\text{ }\mu\text{s}$
  - $R_2^{(1)} = 180 + \lceil 180 / 1000 \rceil \times 97 + \lceil 180 / 2000 \rceil \times 332 = 180 + 97 + 332 = 609\text{ }\mu\text{s}$
  - $R_2^{(2)} = 180 + \lceil 609 / 1000 \rceil \times 97 + \lceil 609 / 2000 \rceil \times 332 = \mathbf{609\text{ }\mu\text{s}} \le D_2 = 10.000\text{ }\mu\text{s}$ (Hội tụ).
- **`shell` ($i=3$)**:
  - $R_3^{(0)} = 420\text{ }\mu\text{s}$
  - $R_3^{(1)} = 420 + \lceil 420 / 1000 \rceil \times 97 + \lceil 420 / 2000 \rceil \times 332 + \lceil 420 / 10000 \rceil \times 180 = 420 + 97 + 332 + 180 = 1.029\text{ }\mu\text{s}$
  - $R_3^{(2)} = 420 + \lceil 1029 / 1000 \rceil \times 97 + \lceil 1029 / 2000 \rceil \times 332 + \lceil 1029 / 10000 \rceil \times 180 = 420 + 2 \times 97 + 332 + 180 = 1.126\text{ }\mu\text{s}$
  - $R_3^{(3)} = 420 + \lceil 1126 / 1000 \rceil \times 97 + \lceil 1126 / 2000 \rceil \times 332 + \lceil 1126 / 10000 \rceil \times 180 = \mathbf{1.126\text{ }\mu\text{s}} \le D_3 = 200.000\text{ }\mu\text{s}$ (Hội tụ).
- **`idle_aux` ($i=4$)**:
  - $R_4^{(0)} = 250\text{ }\mu\text{s}$
  - $R_4^{(1)} = 250 + \lceil 250 / 1000 \rceil \times 97 + \lceil 250 / 2000 \rceil \times 332 + \lceil 250 / 10000 \rceil \times 180 + \lceil 250 / 50000 \rceil \times 420 = 250 + 97 + 332 + 180 + 420 = 1.279\text{ }\mu\text{s}$
  - $R_4^{(2)} = 250 + \lceil 1279 / 1000 \rceil \times 97 + \lceil 1279 / 2000 \rceil \times 332 + \lceil 1279 / 10000 \rceil \times 180 + \lceil 1279 / 50000 \rceil \times 420 = 250 + 2 \times 97 + 332 + 180 + 420 = 1.376\text{ }\mu\text{s}$
  - $R_4^{(3)} = 250 + \lceil 1376 / 1000 \rceil \times 97 + \lceil 1376 / 2000 \rceil \times 332 + \lceil 1376 / 10000 \rceil \times 180 + \lceil 1376 / 50000 \rceil \times 420 = \mathbf{1.376\text{ }\mu\text{s}} \le D_4 = 100.000\text{ }\mu\text{s}$ (Hội tụ).
- *Kiểm tra thời gian hoàn tất của `imu` kể cả chờ bus (self-suspension $S = 433\ \mu s$, coi như thời gian chạy — cách *suspension-oblivious*)*: $R = 332 + 433 + \lceil R/1000 \rceil \times 97$ ⇒ $765 \rightarrow \mathbf{862\ \mu s} \le 2.000\ \mu s$. Jitter hoàn tất $862 - 332 = 530\ \mu s$ đưa vào số hạng $\lceil (R + J)/T \rceil$ của các task thấp hơn không làm đổi kết quả nào ở trên.

- **Task nào có $R_i$ gần $D_i$ nhất (margin nhỏ nhất)?**
  - Xét theo **deadline tuyệt đối ($D_i - R_i$)**: **`control`**, với margin tuyệt đối $D_0 - R_0 = 1.000 - 97 = \mathbf{903\text{ }\mu\text{s}}$; đặc biệt nếu xét theo **ràng buộc Jitter cứng** (`W4E1_CONTROL_LOOP_JITTER_MAX_US = 113 us`), margin của **`control`** chỉ còn **$113 - 97 = \mathbf{16\text{ }\mu\text{s}}$**.
  - Xét theo **tỷ lệ phần trăm ($R_i / D_i$)**: **`imu`** cao nhất ($429 / 2.000 = \mathbf{21,45\%}$, margin $1.571\text{ }\mu\text{s}$); tính cả $433\ \mu s$ chờ bus ở 400 kHz thì hoàn tất ở $862\ \mu s$ ($43{,}1\%$ của $D$), margin **$1.138\ \mu s$**. Nếu giữ 100 kHz thì con số này là $\approx 2.170\ \mu s$ > $D$ — lý do bắt buộc chuyển 400 kHz.

---

## Dự đoán ngân sách

- **Tổng RAM tĩnh dự đoán (bytes), so với `W4E1_RAM_BUDGET_BYTES`**: **`11.776 / 15.684 bytes`** (chiếm **75,08%** ngân sách được giao, còn dư biên an toàn **`3.908 bytes`**; chi tiết từng hạng mục xem trong [REPORT.md](REPORT.md) và `W4E1_RAM_BUDGET_TABLE`).
- **Tổng CPU utilization dự đoán (%), so với `W4E1_CPU_BUDGET_PERCENT_MAX`**:
  - Ở `LO-Mode` danh định: **`18,14%`** (tính riêng 5 task) hoặc **`20,24%`** (cộng thêm $2,10\%$ nền ISR hệ thống `SysTick` + `TIM2` $= 21\ \mu s / 1\ ms$) trên trần **`76%`**.
  - Ở `HI-Stress / Worst-Case` (cả 5 task chạy ở $C_{HI}$): **`29,19%`** (5 task) hoặc **`31,29%`** (tổng cộng kèm nền ISR) **`/ 76%`** (dư biên an toàn CPU **`44,71%`**).

---

## Rủi ro đã lường trước

- **Nếu I2C1 bus-recovery (W2.1) xảy ra đúng lúc control loop tới hạn: dự đoán ảnh hưởng tới WCRT của `control`**:
  - **Không có tài nguyên dùng chung nào chặn `control`** (chỉ còn các critical section ngắn của kernel, chưa đo — REPORT §5 F19) và WCRT của `control` chỉ tăng tối đa từ $70\text{ }\mu\text{s}$ lên **$97\text{ }\mu\text{s}$ ($\le 113\text{ }\mu\text{s}$)** nhờ 3 chốt thiết kế:
    1. Trình tự phát 9 xung SCL + START/STOP ($\approx 120\text{ }\mu\text{s}$) **chạy hoàn toàn trong ngữ cảnh task `imu` (Priority 7)** với ngắt được bật, tuyệt đối không chạy trong `I2C1_ER_IRQHandler` và không bọc trong `taskENTER_CRITICAL()`, nên `TIM3_IRQHandler` và task `control` (Priority 8) preempt `imu` ngay lập tức.
    2. Dữ liệu giữa `imu` và `control` truyền qua **bộ đệm SPSC / Seqlock không khóa** (kế thừa W2.2) thay vì dùng chung `I2C1 Mutex`, nên `control` đọc lại mẫu hợp lệ gần nhất và tăng `imu_stale_count` mà không chờ bus.
    3. Ngắt lỗi `I2C1_ER_IRQHandler` chỉ xóa cờ lỗi, tắt `ITEVFEN/ITERREN` và gọi `vTaskNotifyGiveFromISR()` trong $< 6\text{ }\mu\text{s}$.
- **Nếu mentor không duyệt thiết kế lần đầu: điểm nào trong bảng trên bạn nghĩ sẽ bị yêu cầu sửa, vì sao**:
  - **Điểm nhạy cảm nhất là xung đột giữa `STOP mode` (W3.2) và chu kỳ $1\text{ ms}$ (Jitter $\le 113\text{ }\mu\text{s}$) của `control`**: Dự báo của W3.2 (chưa đo tách riêng được vì chân PA0 chỉ lên mức cao sau khi PLL đã khóa) là thoát `STOP mode` và chờ thạch anh `HSE 8 MHz` + khóa lại `PLL 72 MHz` mất **$\approx 1,71\text{--}2,21\text{ ms}$**, đồng thời `STOP mode` tắt toàn bộ clock APB1/AHB (làm dừng `TIM3`, `TIM2` và `DMA1`). Nếu gọi `STOP mode` giữa hai chu kỳ $1\text{ ms}$ của `control` thì chắc chắn vỡ deadline. Do đó thiết kế chốt cơ chế **Adaptive Sleep Gating**: khi vòng điều khiển `control` 1 kHz đang active (hoặc đang truyền DMA), hook `portSUPPRESS_TICKS_AND_SLEEP` chỉ dùng lệnh `WFI` ở chế độ **`SLEEP mode`** (giữ nguyên HSE/PLL 72 MHz và clock APB, thời gian thức dậy $< 2\text{ }\mu\text{s}$); chỉ khi hệ thống được lệnh shell đưa vào trạng thái Standby/Idle dài hạn ($\ge 5\text{ ticks}$ không có task chu kỳ ngắn) thì mới hạ xuống **`STOP mode`** dùng RTC LSI.
  - **Điểm thứ hai mentor có thể yêu cầu xem lại là quyết định chạy `I2C1` ở 400 kHz**: driver W2.1 chỉ được kiểm ở 100 kHz, nên số lần vào ISR (19) và thời gian bus (≈ 433 µs) đều là suy luận. Ở 400 kHz cần sườn lên SCL ≤ 300 ns: phải đo bằng logic analyzer/oscilloscope với điện trở kéo lên của module, và nếu không đạt thì phương án dự phòng là đọc gyro xen kẽ (250 Hz) ở 100 kHz.
  - **Vòng DMA RX 128 B**: nửa vòng còn 0,64 ms ở 1 Mbps (W3.1 dùng 256 B, nửa vòng 2,78 ms ở 460.800 bps), chỉ gấp 1,28 lần ngân sách ISR 0,5 ms của W3.1; khi dây bận liên tục, ngắt HT/TC lên tới ≈ 1.560 lần/s × 13,7 µs ≈ 2,1 % CPU. W4.2 phải kiểm `dma_late = 0` và `dropped = 0` ở 1 Mbps.
