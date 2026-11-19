# W4.1 – PREDICTION (điền TRƯỚC khi nộp design doc cho mentor duyệt)

- **MSSV**: `2452882` | **MCU**: `STM32F103C8T6` (Cortex-M3 @ 72 MHz, 20 KiB SRAM)
- **Nguồn tham số**: `include/params.h` và `PARAMS.md` (sinh bằng `py tools/gen_params.py 2452882`, bị `.gitignore`) và SSOT [include/mixed_criticality_design.h](include/mixed_criticality_design.h).
- **Quy ước nhãn nguồn**: **Đo** = trích số đo thật từ bài tuần trước; **Suy luận** = tính từ số đo/ngân sách đã kiểm của tuần trước; **Ước lượng** = chưa có số đo, W4.2 phải đo bằng DWT và đối chiếu sai số `W4E3_WCRT_TOLERANCE_PERCENT = 12%`.

**Nhật ký sửa đổi trước khi mentor duyệt** *(mọi lần sửa đều diễn ra trước khi duyệt và trước khi có số đo W4.2)*:

| Ngày | Thay đổi chính | Lý do kỹ thuật |
|---|---|---|
| 18/11/2026 | Bản đầu; hiệu chỉnh $C_{HI}(control)$ $90 \rightarrow 97\text{ }\mu\text{s}$ | Cộng đúng các thành phần phân rã ($70 + 11 + 16 = 97\text{ }\mu\text{s}$) |
| 19/11/2026 | Nâng `I2C1` lên 400 kHz đọc burst 14 B ($C_{imu}: 165/332\text{ }\mu\text{s}$); giảm vòng DMA RX $256 \rightarrow 128\text{ B}$ (RAM $11.776\text{ B}$); sửa nền ISR $2,10\%$, ngưỡng stale $> 3\text{ tick}$ | Đủ accel + gyro trong chu kỳ 2 ms; giữ ISR UART RX $\le 16\text{ }\mu\text{s}$; loại báo stale giả |
| 19/11/2026 | Vá `UART_FLOOD` mask thêm `DMA1_Ch5 HTIE/TCIE`; chuẩn hóa bảng nguồn kế thừa theo từng Week | Dập bão ngắt 1.562,5 Hz khi flood liên tục 1 Mbps; minh bạch nguồn W1–W3 |

---

## 1. Dự đoán RTA (Bảng T / D / C / R khớp `W4E1_RTA_TABLE`)

| Role | Criticality | $T$ ($\mu\text{s}$) | $D$ ($\mu\text{s}$) | $C_{LO}$ ($\mu\text{s}$) | $C_{HI}$ / `wcet_us` ($\mu\text{s}$) | $R_{LO}$ dự đoán ($\mu\text{s}$) | $R_{HI}$ / `response_time_us` ($\mu\text{s}$) | Margin $D_i - R_{HI}$ ($\mu\text{s}$) | Priority |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `control` | **HI** | 1.000 | 1.000 | 70 | **97** | 70 | **97** | 903 *(Jitter margin: **16**)* | **8** |
| `imu` | **HI** | 2.000 | 2.000 | 165 | **332** | 235 | **429** | 1.571 *(E2E chờ bus: 862 $\rightarrow$ **1.138**)* | **7** |
| `telemetry` | **LO** | 10.000 | 10.000 | 180 | **180** *(Shed $\rightarrow 0$)* | 415 | **609** *(Shed $\rightarrow$ N/A)* | 9.391 | **5** |
| `shell` | **LO** | 50.000 | 200.000 | 420 | **420** *(Shed $\rightarrow 0$)* | 835 | **1.126** *(Shed $\rightarrow$ N/A)* | 198.874 | **3** |
| `idle_aux` | **LO** | 100.000 | 100.000 | 250 | **250** *(HI $\rightarrow 80$)* | 1.155 | **1.376** *(HI $\rightarrow 509$)* | 98.624 | **2** |

### 1.1. Phân rã WCET $C_i$ (72 MHz, `-O2`) và Nguồn Kế thừa theo từng Week

| Vai trò (Prio) | Thành phần công việc | $\mu\text{s}$ | Nhãn | Nguồn Week & Căn cứ |
|---|---|---:|---|---|
| **`control`** (P8, 1 kHz) | `TIM3_IRQHandler` + `vTaskNotifyGiveFromISR` + context switch | 8 | Suy luận | **W1.2** (OUT$\rightarrow$IN 219–244 cycles $\approx 3,0\text{--}3,4\text{ }\mu\text{s}$ `-O2`) |
| | Đọc mẫu `w4e2_imu_sample_t` qua SPSC seqlock | 1 | Đo | **W2.2** (pop 24 cycles $\approx 0,33\text{ }\mu\text{s}$, làm tròn lên) |
| | Bộ lọc bổ sung + PID 3 trục + cập nhật `status` & heartbeat | 61 | Ước lượng | **W4.2** (PID 56 + status/HB 5; đo lại bằng DWT) |
| | $\Rightarrow$ **$C_{LO}$ / Cộng HI**: nhánh safe-hold (+11) + 1 ISR UART RX (+16) | **70 / 97** | Suy luận | **W3.1** (220 + 12$\times$64 B = 988 cycles $\approx 13,7\text{ }\mu\text{s} \le 16\text{ }\mu\text{s}$) |
| **`imu`** (P7, 500 Hz) | Đánh thức sau `TIM3 ÷2` & `I2C1` (7) + quy đổi 14 B & ghi SPSC (98) | 105 | Ước lượng | **W2.2 / W4.2** (mẫu cần cả accel + gyro $\rightarrow$ burst 14 B) |
| | 19 lần vào ISR `I2C1_EV/ER` (ghi 4 + đọc 14 B: 15) $\times$ 3,18 / 5,583 $\mu\text{s}$ | 60 / 107 | Suy luận | **W2.1** (marker EV 1,000–5,583 $\mu\text{s}$; nâng `I2C1` lên 400 kHz) |
| | Giải cứu kẹt SDA trong task `imu`: 9 xung SCL (90) + START/STOP (17,75) + re-init (12) | +120 | Suy luận | **W2.1** (START/STOP đã đo; nhánh 9 xung `PULSES=0` chưa chạy) |
| | $\Rightarrow$ **$C_{LO}$ / $C_{HI}$** *(Chờ bus 400 kHz: 433 $\mu\text{s}$, task Blocked, không thuộc $C$)* | **165 / 332** | Suy luận | **W2.1** (`CCR=30`, `TRISE=11`; ở 100 kHz chờ 1.619 $\mu\text{s}$ sẽ vỡ hạn) |
| **`telemetry`** (P5, 100 Hz) | CRC32 khung 70 B (107) + đóng gói, nạp `DMA1_Ch4` TX 1 Mbps & TC (73) | **180** | Suy luận / ƯL | **W3.1** (110 cycles/B đã kiểm gián tiếp; DMA phát không tốn CPU) |
| **`shell`** (P3, $T_{min}$ 50 ms) | Parser + CRC32 + `shell_exec()` (350) + format reply (45) + ISR RX (25) | **420** | Ước lượng / SL | **W3.1 / W4.2** (ISR khung 70 B: $2\times 220 + 70\times 12 = 1.280$ cyc $\approx 18\text{ }\mu\text{s}$) |
| **`idle_aux`** (P2, 100 ms) | Kiểm tra heartbeat, feed `IWDG` (180) + `TIM2` stats & 5$\times$HWM (50) + LSI cal (20) | **250** | Ước lượng | **W1.4 / W2.4 / W3.1 / W3.2** (HI-Mode rút xuống 80 $\mu\text{s}$) |

### 1.2. Các bước lặp hội tụ RTA bằng tay ($R_i^{(k+1)} = C_i + \sum_{j \in hp(i)} \lceil R_i^{(k)} / T_j \rceil C_j$, chế độ `HI-Stress`)
- **`control` ($i=0$)**: $R_0 = C_0 = \mathbf{97\text{ }\mu\text{s}} \le 1.000\text{ }\mu\text{s}$ (và $\le J_{max} = 113\text{ }\mu\text{s}$).
- **`imu` ($i=1$)**: $332 \rightarrow 332 + \lceil 332/1000 \rceil \cdot 97 = \mathbf{429\text{ }\mu\text{s}} \le 2.000\text{ }\mu\text{s}$.
- **`telemetry` ($i=2$)**: $180 \rightarrow 180 + 97 + 332 = \mathbf{609\text{ }\mu\text{s}} \le 10.000\text{ }\mu\text{s}$.
- **`shell` ($i=3$)**: $420 \rightarrow 420 + 97 + 332 + 180 = 1.029 \rightarrow 420 + 2\cdot 97 + 332 + 180 = \mathbf{1.126\text{ }\mu\text{s}} \le 200.000\text{ }\mu\text{s}$ (và $\le T_{min}=50\text{ ms}$ nên RTA cổ điển hợp lệ).
- **`idle_aux` ($i=4$)**: $250 \rightarrow 250 + 97 + 332 + 180 + 420 = 1.279 \rightarrow 250 + 2\cdot 97 + 332 + 180 + 420 = \mathbf{1.376\text{ }\mu\text{s}} \le 100.000\text{ }\mu\text{s}$.
- **Kiểm tra hoàn tất `imu` có chờ bus** (*suspension-oblivious*, $S = 433\text{ }\mu\text{s}$): $R = 332 + 433 + \lceil R/1000 \rceil \cdot 97 \Rightarrow 765 \rightarrow \mathbf{862\text{ }\mu\text{s}} \le 2.000\text{ }\mu\text{s}$. Jitter hoàn tất $J_{imu} = 862 - 332 = 530\text{ }\mu\text{s}$ không làm đổi $R$ của các task thấp hơn.

- **Task có $R_i$ gần $D_i$ nhất (margin nhỏ nhất)**:
  - Theo **biên tuyệt đối ($D_i - R_i$)**: **`control`**, margin $1.000 - 97 = \mathbf{903\text{ }\mu\text{s}}$; riêng theo **trần Jitter cứng** (`113 µs`), margin chỉ còn **$113 - 97 = \mathbf{16\text{ }\mu\text{s}}$**.
  - Theo **tỷ lệ ($R_i / D_i$)**: **`imu`** cao nhất ($429/2.000 = 21,45\%$; tính cả chờ bus là $862/2.000 = 43,1\%$, margin **$1.138\text{ }\mu\text{s}$**).

---

## 2. Dự đoán Ngân sách Tài nguyên Tĩnh

- **Tổng RAM tĩnh dự đoán (`W4E1_RAM_BUDGET_BYTES`)**: **`11.776 / 15.684 bytes`** (chiếm **75,08%**, dư biên **`3.908 bytes`**; phân bổ 18 mục gắn nguồn W1.2–W3.3 chi tiết trong [REPORT.md](REPORT.md) §2.2 và `W4E1_RAM_BUDGET_TABLE`).
- **Tổng CPU utilization dự đoán (`W4E1_CPU_BUDGET_PERCENT_MAX`)**:
  - `LO-Mode` danh định: **`18,14%`** (5 task) + **`2,10%`** (nền ISR `SysTick` W1.3 + `TIM2` W3.1 = $21\text{ }\mu\text{s}/1\text{ ms}$) = **`20,24% / 76%`**.
  - `HI-Stress / Worst-Case`: **`29,19%`** (5 task) + **`2,10%`** (nền ISR) = **`31,29% / 76%`** (dư **`44,71%`**; khi shed `telemetry` + `shell` ở `HI-Mode` thực tế còn $26,38\% + 2,10\% = 28,48\%$).

---

## 3. Rủi ro đã lường trước

1. **Ảnh hưởng của I2C1 bus-recovery (W2.1) lên WCRT của `control`**:
   - Không có tài nguyên dùng chung nào chặn `control` (chỉ còn critical section ngắn của kernel, đo ở W4.3); WCRT của `control` tăng tối đa $70 \rightarrow \mathbf{97\text{ }\mu\text{s}} \le 113\text{ }\mu\text{s}$ nhờ: (a) chuỗi 9 xung SCL chạy trong task `imu` (P7) với ngắt mở, `control` (P8) preempt tức thời; (b) truyền mẫu qua SPSC seqlock không khóa (W2.2); (c) `I2C1_ER_IRQHandler` chỉ xóa cờ, tắt ngắt I2C và gọi `xTaskNotifyFromISR` ($< 6\text{ }\mu\text{s}$).
2. **Các điểm rủi ro mentor có thể yêu cầu rà soát khi duyệt thiết kế**:
   - **Xung đột `STOP mode` (W3.2) vs `TIM3` 1 kHz**: Dự báo W3.2 thoát STOP + khóa lại `HSE/PLL 72 MHz` mất $1,71\text{--}2,21\text{ ms}$ và STOP dừng APB (`TIM3`, `DMA1`). Giải pháp: **Adaptive Sleep Gating** — bình thường giữa nhịp 1 ms chỉ dùng **`SLEEP mode` (`WFI`, trễ $< 2\text{ }\mu\text{s}$)**; chỉ hạ `STOP mode` qua `RTC LSI` khi lệnh shell đưa hệ thống về trạng thái rảnh dài hạn ($\ge 5\text{ ticks}$).
   - **Nâng `I2C1` lên 400 kHz (W2.1 chỉ đo 100 kHz)**: W4.2 phải đo lại sườn lên SCL ($\le 300\text{ ns}$) bằng logic analyzer; nếu phần cứng kéo lên yếu, phương án dự phòng là giảm tần số đọc gyro xen kẽ (250 Hz) ở 100 kHz.
   - **Thu hẹp vòng DMA RX về 128 B (W3.1 dùng 256 B)**: Nửa vòng còn $0,64\text{ ms}$ ở 1 Mbps (ngắt HT/TC tối đa $1.562,5\text{ Hz} \approx 2,14\%$ CPU khi flood liên tục), nên khi phát hiện `UART_FLOOD` phải mask cả `USART1 IDLEIE` **lẫn `DMA1_Ch5 HTIE/TCIE`** trong $50\text{ ms}$ cooldown; W4.2 phải kiểm chứng `dma_late = 0`, `dropped = 0`.
