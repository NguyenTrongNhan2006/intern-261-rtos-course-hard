# W4.1 – PREDICTION (điền TRƯỚC khi nộp design doc cho mentor duyệt)

MSSV: **2452882** | MCU: **STM32F103C8T6** (Cortex-M3 @ 72 MHz, 20 KiB SRAM) | Nguồn tham số: [include/params.h](include/params.h), [PARAMS.md](PARAMS.md) và [include/mixed_criticality_design.h](include/mixed_criticality_design.h).

---

## Dự đoán RTA

- Bảng $T / D / C / R$ cho 5 vai trò (khớp nguyên văn mảng `W4E1_RTA_TABLE` trong [include/mixed_criticality_design.h](include/mixed_criticality_design.h)), điền bằng tay TRƯỚC khi mentor duyệt:

| Role | Criticality | $T$ ($\mu\text{s}$) | $D$ ($\mu\text{s}$) | $C_{LO}$ ($\mu\text{s}$) | $C_{HI}$ / `wcet_us` ($\mu\text{s}$) | $R_{LO}$ dự đoán ($\mu\text{s}$) | $R_{HI}$ / `response_time_us` ($\mu\text{s}$) | Margin $D_i - R_{HI}$ ($\mu\text{s}$) | Priority |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `control` | **HI** | 1.000 | 1.000 | 70 | **90** | 70 | **90** | 910 (Jitter margin: **23**) | **8** |
| `imu` | **HI** | 2.000 | 2.000 | 140 | **260** | 210 | **350** | 1.650 | **7** |
| `telemetry` | **LO** | 10.000 | 10.000 | 180 | **180** (Shed $\rightarrow 0$) | 390 | **530** (Shed $\rightarrow$ N/A) | 9.470 | **5** |
| `shell` | **LO** | 50.000 | 200.000 | 420 | **420** (Shed $\rightarrow 0$) | 810 | **950** (Shed $\rightarrow$ N/A) | 199.050 | **3** |
| `idle_aux` | **LO** | 100.000 | 100.000 | 250 | **250** (HI $\rightarrow 80$) | 1.130 | **1.290** (HI $\rightarrow 430$) | 98.710 | **2** |

### Diễn giải nguồn gốc $C_i$ từ số đo thực nghiệm W1–W3 (72 MHz, `-O2`)
1. **`control` (`W4E1_ROLE_CONTROL`, Priority 8, 1 kHz)**:
   - Kích hoạt bởi ngắt so sánh `TIM3_IRQn` (1 kHz).
   - Ở `LO-Mode`: Ngắt `TIM3` + chuyển ngữ cảnh (`244 cycles` $\approx 3,4\text{ }\mu\text{s}$ từ W1.2, tổng $8\text{ }\mu\text{s}$) + đọc mẫu IMU không khóa qua `spsc_ring` / Seqlock (`24 cycles` $\approx 0,34\text{ }\mu\text{s}$ từ W2.2, làm tròn $1\text{ }\mu\text{s}$) + bộ lọc bổ sung & luật điều khiển PID 3 trục ($56\text{ }\mu\text{s}$) + cập nhật `w4e2_system_status_t` & bit heartbeat trên `EventGroup` ($5\text{ }\mu\text{s}$) $\Rightarrow C_{LO} = \mathbf{70\text{ }\mu\text{s}}$.
   - Ở `HI-Mode` (Worst-Case): Cộng thêm nhánh kiểm tra độ tươi mẫu (`sample_tick`), dự phóng tư thế an toàn (safe-hold attitude extrapolation) khi mất cảm biến và chi phí che chắn tối đa 1 ngắt ngoại vi ngắn $\Rightarrow C_{HI} = \mathbf{90\text{ }\mu\text{s}}$.
2. **`imu` (`W4E1_ROLE_IMU`, Priority 7, 500 Hz)**:
   - Đọc cảm biến MPU6050 qua driver `I2C1` ngắt (`EV/ER`) từ W2.1. Trong thời gian phần cứng dịch bit trên bus vật lý ($\approx 647\text{--}880\text{ }\mu\text{s}$ ở 100 kHz), task `imu` ở trạng thái **Blocked** trên `ulTaskNotifyTake()`, giải phóng hoàn toàn CPU cho các task khác.
   - Ở `LO-Mode`: Khởi tạo giao dịch + 11 lần vào ISR `I2C1_EV/ER` ($1,0\text{--}5,58\text{ }\mu\text{s}$/lần từ W2.1 $\approx 35\text{ }\mu\text{s}$) + đánh thức task ($7\text{ }\mu\text{s}$) + quy đổi dữ liệu thô sang `w4e2_imu_sample_t` và ghi vào SPSC snapshot ($98\text{ }\mu\text{s}$) $\Rightarrow C_{LO} = \mathbf{140\text{ }\mu\text{s}}$.
   - Ở `HI-Mode` (Worst-Case có **I2C1 Bus Recovery**): Cộng thêm trình tự giải cứu kẹt SDA trong ngữ cảnh task `imu` (chuyển PB6/PB7 sang GPIO open-drain, phát tối đa 9 xung SCL $\approx 90\text{ }\mu\text{s}$ + xung START/STOP $17,75\text{ }\mu\text{s}$ đo ở W2.1 + tái cấu hình thanh ghi `I2C1` $\approx 12\text{ }\mu\text{s}$) $\Rightarrow C_{HI} = \mathbf{260\text{ }\mu\text{s}}$.
3. **`telemetry` (`W4E1_ROLE_TELEMETRY`, Priority 5, 100 Hz)**:
   - Đọc snapshot trạng thái, đóng gói khung telemetry 70 B, tính CRC32 phần mềm ($110\text{ cycles/B}$ từ W3.1 $\approx 107\text{ }\mu\text{s}$) và nạp thanh ghi `DMA1_Channel4` phát qua `USART1` ở `1.000.000 bps` kèm ngắt TC $\Rightarrow C_{LO} = C_{HI} = \mathbf{180\text{ }\mu\text{s}}$ (khi chuyển sang `HI-Mode` do quá tải, task này được cắt tải $C_{HI\_shed} = 0\text{ }\mu\text{s}$).
4. **`shell` (`W4E1_ROLE_SHELL`, Priority 3, Sporadic $T_{min} = 50\text{ ms}$, $D = 200\text{ ms}$)**:
   - Nhận luồng byte từ `StreamBuffer` (được điền bởi `DMA1_Channel5` + `USART1 IDLE` ISR từ W3.1), chạy parser tách khung + CRC32 + thực thi `mixed_criticality_system_shell_exec()` và tạo chuỗi phản hồi `reply_buf` $\Rightarrow C_{LO} = C_{HI} = \mathbf{420\text{ }\mu\text{s}}$.
5. **`idle_aux` (`W4E1_ROLE_IDLE_AUX`, Priority 2, 10 Hz / $T = 100\text{ ms}$)**:
   - Thu thập thống kê `uxTaskGetSystemState()` từ bộ đếm `TIM2` 100 kHz (W3.1), kiểm tra `uxTaskGetStackHighWaterMark()` cho 5 task, kiểm tra mặt nạ heartbeat trên `EventGroup` để nạp lại `IWDG` (trước hạn 296 ms), và cập nhật tham số hiệu chuẩn LSI cho nhánh ngủ `Tickless Idle` (W3.2) $\Rightarrow C_{LO} = C_{HI} = \mathbf{250\text{ }\mu\text{s}}$.

### Từng bước lặp hội tụ RTA bằng tay ($R_i^{(k+1)} = C_i + \sum_{j \in hp(i)} \lceil R_i^{(k)} / T_j \rceil C_j$)
- **`control` ($i=0$)**: $hp(0) = \emptyset \Rightarrow R_0^{(0)} = C_0 = \mathbf{90\text{ }\mu\text{s}} \le D_0 = 1.000\text{ }\mu\text{s}$ (và $\le J_{max} = 113\text{ }\mu\text{s}$).
- **`imu` ($i=1$)**:
  - $R_1^{(0)} = 260\text{ }\mu\text{s}$
  - $R_1^{(1)} = 260 + \lceil 260 / 1000 \rceil \times 90 = 260 + 1 \times 90 = \mathbf{350\text{ }\mu\text{s}}$
  - $R_1^{(2)} = 260 + \lceil 350 / 1000 \rceil \times 90 = \mathbf{350\text{ }\mu\text{s}} \le D_1 = 2.000\text{ }\mu\text{s}$ (Hội tụ).
- **`telemetry` ($i=2$)**:
  - $R_2^{(0)} = 180\text{ }\mu\text{s}$
  - $R_2^{(1)} = 180 + \lceil 180 / 1000 \rceil \times 90 + \lceil 180 / 2000 \rceil \times 260 = 180 + 90 + 260 = \mathbf{530\text{ }\mu\text{s}}$
  - $R_2^{(2)} = 180 + \lceil 530 / 1000 \rceil \times 90 + \lceil 530 / 2000 \rceil \times 260 = \mathbf{530\text{ }\mu\text{s}} \le D_2 = 10.000\text{ }\mu\text{s}$ (Hội tụ).
- **`shell` ($i=3$)**:
  - $R_3^{(0)} = 420\text{ }\mu\text{s}$
  - $R_3^{(1)} = 420 + \lceil 420 / 1000 \rceil \times 90 + \lceil 420 / 2000 \rceil \times 260 + \lceil 420 / 10000 \rceil \times 180 = 420 + 90 + 260 + 180 = \mathbf{950\text{ }\mu\text{s}}$
  - $R_3^{(2)} = 420 + \lceil 950 / 1000 \rceil \times 90 + \lceil 950 / 2000 \rceil \times 260 + \lceil 950 / 10000 \rceil \times 180 = \mathbf{950\text{ }\mu\text{s}} \le D_3 = 200.000\text{ }\mu\text{s}$ (Hội tụ).
- **`idle_aux` ($i=4$)**:
  - $R_4^{(0)} = 250\text{ }\mu\text{s}$
  - $R_4^{(1)} = 250 + \lceil 250 / 1000 \rceil \times 90 + \lceil 250 / 2000 \rceil \times 260 + \lceil 250 / 10000 \rceil \times 180 + \lceil 250 / 50000 \rceil \times 420 = 250 + 90 + 260 + 180 + 420 = 1.200\text{ }\mu\text{s}$
  - $R_4^{(2)} = 250 + \lceil 1200 / 1000 \rceil \times 90 + \lceil 1200 / 2000 \rceil \times 260 + \lceil 1200 / 10000 \rceil \times 180 + \lceil 1200 / 50000 \rceil \times 420 = 250 + 2 \times 90 + 260 + 180 + 420 = \mathbf{1.290\text{ }\mu\text{s}}$
  - $R_4^{(3)} = 250 + \lceil 1290 / 1000 \rceil \times 90 + \dots = \mathbf{1.290\text{ }\mu\text{s}} \le D_4 = 100.000\text{ }\mu\text{s}$ (Hội tụ sau khi bị `control` preempt lần thứ 2 tại mốc $1.000\text{ }\mu\text{s}$).

- **Task nào có $R_i$ gần $D_i$ nhất (margin nhỏ nhất)?**
  - Xét theo **deadline tuyệt đối ($D_i - R_i$)**: **`control`**, với margin tuyệt đối $D_0 - R_0 = 1.000 - 90 = \mathbf{910\text{ }\mu\text{s}}$; đặc biệt nếu xét theo **ràng buộc Jitter cứng** (`W4E1_CONTROL_LOOP_JITTER_MAX_US = 113 us`), margin của **`control`** chỉ còn **$113 - 90 = \mathbf{23\text{ }\mu\text{s}}$**.
  - Xét theo **tỷ lệ phần trăm ($R_i / D_i$)**: **`imu`** chiếm tỷ lệ cao nhất về CPU ($350 / 2.000 = \mathbf{17,5\%}$, margin $1.650\text{ }\mu\text{s}$; hoặc nếu cộng cả $880\text{ }\mu\text{s}$ thời gian truyền vật lý trên dây I2C1 ở 100 kHz thì hoàn tất giao dịch ở $1.230\text{ }\mu\text{s}$, margin còn **$770\text{ }\mu\text{s}$**).

---

## Dự đoán ngân sách

- **Tổng RAM tĩnh dự đoán (bytes), so với `W4E1_RAM_BUDGET_BYTES`**: **`11.904 / 15.684 bytes`** (chiếm **75,90%** ngân sách được giao, còn dư biên an toàn **`3.780 bytes`**; chi tiết từng hạng mục xem trong [REPORT.md](REPORT.md) và `W4E1_RAM_BUDGET_TABLE`).
- **Tổng CPU utilization dự đoán (%), so với `W4E1_CPU_BUDGET_PERCENT_MAX`**:
  - Ở `LO-Mode` danh định: **`16,89%`** (tính riêng 5 task) hoặc **`19,00%`** (cộng thêm $2,11\%$ nền ISR hệ thống `SysTick` + `TIM2`) trên trần **`76%`**.
  - Ở `HI-Stress / Worst-Case` (cả 5 task chạy ở $C_{HI}$): **`24,89%`** (5 task) hoặc **`27,00%`** (tổng cộng kèm nền ISR) **`/ 76%`** (dư biên an toàn CPU **`49,00%`**).

---

## Rủi ro đã lường trước

- **Nếu I2C1 bus-recovery (W2.1) xảy ra đúng lúc control loop tới hạn: dự đoán ảnh hưởng tới WCRT của `control`**:
  - **Không làm `control` bị chặn ($B_{control} = 0\text{ }\mu\text{s}$)** và WCRT của `control` chỉ tăng tối đa từ $70\text{ }\mu\text{s}$ lên **$90\text{ }\mu\text{s}$ ($\le 113\text{ }\mu\text{s}$)** nhờ 3 chốt thiết kế:
    1. Trình tự phát 9 xung SCL + START/STOP ($\approx 120\text{ }\mu\text{s}$) **chạy hoàn toàn trong ngữ cảnh task `imu` (Priority 7)** với ngắt được bật, tuyệt đối không chạy trong `I2C1_ER_IRQHandler` và không bọc trong `taskENTER_CRITICAL()`, nên `TIM3_IRQHandler` và task `control` (Priority 8) preempt `imu` ngay lập tức.
    2. Dữ liệu giữa `imu` và `control` truyền qua **bộ đệm SPSC / Seqlock không khóa** (kế thừa W2.2) thay vì dùng chung `I2C1 Mutex`, nên `control` đọc lại mẫu hợp lệ gần nhất và tăng `imu_stale_count` mà không chờ bus.
    3. Ngắt lỗi `I2C1_ER_IRQHandler` chỉ xóa cờ lỗi, tắt `ITEVFEN/ITERREN` và gọi `vTaskNotifyGiveFromISR()` trong $< 6\text{ }\mu\text{s}$.
- **Nếu mentor không duyệt thiết kế lần đầu: điểm nào trong bảng trên bạn nghĩ sẽ bị yêu cầu sửa, vì sao**:
  - **Điểm nhạy cảm nhất là xung đột giữa `STOP mode` (W3.2) và chu kỳ $1\text{ ms}$ (Jitter $\le 113\text{ }\mu\text{s}$) của `control`**: Thực nghiệm W3.2 cho thấy thoát `STOP mode` và chờ thạch anh `HSE 8 MHz` + khóa lại `PLL 72 MHz` mất **$1,71\text{--}2,21\text{ ms}$**, đồng thời `STOP mode` tắt toàn bộ clock APB1/AHB (làm dừng `TIM3`, `TIM2` và `DMA1`). Nếu gọi `STOP mode` giữa hai chu kỳ $1\text{ ms}$ của `control` thì chắc chắn vỡ deadline. Do đó trong thiết kế này, chúng tôi đã chủ động chốt cơ chế **Adaptive Sleep Gating**: khi vòng điều khiển `control` 1 kHz đang active (hoặc đang truyền DMA), hook `portSUPPRESS_TICKS_AND_SLEEP` chỉ dùng lệnh `WFI` ở chế độ **`SLEEP mode`** (giữ nguyên HSE/PLL 72 MHz và clock APB, thời gian thức dậy $< 2\text{ }\mu\text{s}$); chỉ khi hệ thống được lệnh shell đưa vào trạng thái Standby/Idle dài hạn ($\ge 5\text{ ticks}$ không có task chu kỳ ngắn) thì mới hạ xuống **`STOP mode`** dùng RTC LSI.
