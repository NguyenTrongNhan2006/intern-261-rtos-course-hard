# W4.1 – PREDICTION (điền TRƯỚC khi nộp design doc cho mentor duyệt)

MSSV: **2452882** | MCU: **STM32F103C8T6** (Cortex-M3 @ 72 MHz, 20 KiB SRAM) | Nguồn tham số: [include/params.h](include/params.h), [PARAMS.md](PARAMS.md) và [include/mixed_criticality_design.h](include/mixed_criticality_design.h).

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

### Diễn giải nguồn gốc $C_i$ từ số đo thực nghiệm W1–W3 (72 MHz, `-O2`)
1. **`control` (`W4E1_ROLE_CONTROL`, Priority 8, 1 kHz)**:
   - Kích hoạt bởi ngắt so sánh `TIM3_IRQn` (1 kHz).
   - Ở `LO-Mode`: Ngắt `TIM3` + chuyển ngữ cảnh (`244 cycles` $\approx 3,4\text{ }\mu\text{s}$ từ W1.2, tổng $8\text{ }\mu\text{s}$) + đọc mẫu IMU không khóa qua `spsc_ring` / Seqlock (`24 cycles` $\approx 0,34\text{ }\mu\text{s}$ từ W2.2, làm tròn $1\text{ }\mu\text{s}$) + bộ lọc bổ sung & luật điều khiển PID 3 trục ($56\text{ }\mu\text{s}$) + cập nhật `w4e2_system_status_t` & bit heartbeat trên `EventGroup` ($5\text{ }\mu\text{s}$) $\Rightarrow C_{LO} = \mathbf{70\text{ }\mu\text{s}}$.
   - Ở `HI-Mode` (Worst-Case): Cộng thêm nhánh kiểm tra độ tươi mẫu (`sample_tick`), dự phóng tư thế an toàn (safe-hold attitude extrapolation) khi mất cảm biến ($+11\text{ }\mu\text{s}$) và chi phí bị chiếm quyền bởi tối đa 1 ngắt ngoại vi dài nhất (`USART1/DMA` $\le 16\text{ }\mu\text{s}$) $\Rightarrow C_{HI} = 70 + 11 + 16 = \mathbf{97\text{ }\mu\text{s}}$ (bản audit 18/11/2026 sửa từ 90: phân rã cũ không cộng đúng).
2. **`imu` (`W4E1_ROLE_IMU`, Priority 7, 500 Hz, đánh thức bởi `TIM3_IRQHandler` mỗi 2 tick — cùng pha với `control`)**:
   - Mẫu `w4e2_imu_sample_t` cần cả `accel_g[3]` và `gyro_dps[3]` ⇒ đọc burst **14 byte** MPU6050 từ `0x3B` tới `0x48` (accel 6 + temp 2 + gyro 6). W2.1 chỉ đo đọc **6 byte** (`ACCEL`, 7 lần vào ISR, 646,583 µs) — bản trước dùng nhầm số này cho cả mẫu accel + gyro (xem REPORT §5, F14).
   - **Bus `I2C1` chạy Fast-mode 400 kHz** (quyết định audit 19/11/2026; MPU6050 hỗ trợ 400 kHz; `CCR = 30` với `F/S = 1, DUTY = 0`, `TRISE = 11` ở `PCLK1 = 36 MHz`). Ngoại suy từ số đo W2.1 (92,37 µs/byte ở 100 kHz, chia 4): ghi thanh ghi $49{,}2$ + khoảng nghỉ phần mềm $36{,}5$ + đọc 15 byte (địa chỉ + 14 dữ liệu) $346{,}4$ $pprox \mathbf{433\ \mu s}$ trên dây, so với $pprox 1.619\ \mu s$ nếu giữ 100 kHz. Trong thời gian này `imu` **Blocked** trên `ulTaskNotifyTake()`.
   - Ở `LO-Mode`: 19 lần vào ISR `I2C1_EV/ER` (4 cho pha ghi + 15 cho pha đọc 14 byte: `SB`, `ADDR`, 12 × `BTF`, `RXNE` — suy từ mẫu 7 lần/6 byte của W2.1) × trung bình $3{,}18\ \mu s$ $pprox 60\ \mu s$ + đánh thức task ($7\ \mu s$) + quy đổi dữ liệu thô sang `w4e2_imu_sample_t` và ghi SPSC ($98\ \mu s$) $\Rightarrow C_{LO} = \mathbf{165\ \mu s}$.
   - Ở `HI-Mode` (Worst-Case có **I2C1 Bus Recovery**): 19 lần ISR lấy **trần** $5{,}583\ \mu s$/lần $pprox 107\ \mu s$ thay cho trung bình, cộng trình tự giải cứu kẹt SDA trong ngữ cảnh task `imu` (GPIO open-drain, tối đa 9 xung SCL $pprox 90\ \mu s$ phát chậm ở 100 kHz cho an toàn + START/STOP $17{,}75\ \mu s$ đo ở W2.1 + tái cấu hình `I2C1` $pprox 12\ \mu s$ $= 120\ \mu s$) $\Rightarrow C_{HI} = 7 + 98 + 107 + 120 = \mathbf{332\ \mu s}$.
   - *Khoảng trống dữ liệu*: nhánh phát 9 xung SCL **chưa từng được kích hoạt** ở W2.1 (`PULSES=0` trong cả 3 log); lượt `RECOVERED` nhanh đo được $153	ext{--}427\ \mu s$ wall-clock (chưa tách phần CPU); số lần vào ISR khi đọc 14 byte và thời gian bus ở 400 kHz là **suy luận**, W4.2 phải đo lại. Phân tích độ nhạy ở [REPORT.md §5.3](REPORT.md) cho thấy với cận bảo thủ $C_{HI}(imu) = 729\ \mu s$ tập task vẫn schedulable.
3. **`telemetry` (`W4E1_ROLE_TELEMETRY`, Priority 5, 100 Hz)**:
   - Đọc snapshot trạng thái, đóng gói khung telemetry 70 B, tính CRC32 phần mềm ($110\text{ cycles/B}$ — ngân sách của W3.1, kiểm chứng gián tiếp: Parser đo $0,78\%$ so với dự đoán $0,80\%$ ở 460.800 bps $\approx 107\text{ }\mu\text{s}$) và nạp thanh ghi `DMA1_Channel4` phát qua `USART1` ở `1.000.000 bps` kèm ngắt TC $\Rightarrow C_{LO} = C_{HI} = \mathbf{180\text{ }\mu\text{s}}$ (khi chuyển sang `HI-Mode` do quá tải, task này được cắt tải $C_{HI\_shed} = 0\text{ }\mu\text{s}$).
4. **`shell` (`W4E1_ROLE_SHELL`, Priority 3, Sporadic $T_{min} = 50\text{ ms}$, $D = 200\text{ ms}$)**:
   - Nhận luồng byte từ `StreamBuffer` (được điền bởi `DMA1_Channel5` + `USART1 IDLE` ISR từ W3.1), chạy parser tách khung + CRC32 + thực thi `mixed_criticality_system_shell_exec()` và tạo chuỗi phản hồi `reply_buf` $\Rightarrow C_{LO} = C_{HI} = \mathbf{420\text{ }\mu\text{s}}$.
5. **`idle_aux` (`W4E1_ROLE_IDLE_AUX`, Priority 2, 10 Hz / $T = 100\text{ ms}$)**:
   - Thu thập thống kê `uxTaskGetSystemState()` từ bộ đếm `TIM2` 100 kHz (W3.1), kiểm tra `uxTaskGetStackHighWaterMark()` cho 5 task, kiểm tra mặt nạ heartbeat trên `EventGroup` để nạp lại `IWDG` (trước hạn 296 ms), và cập nhật tham số hiệu chuẩn LSI cho nhánh ngủ `Tickless Idle` (W3.2) $\Rightarrow C_{LO} = C_{HI} = \mathbf{250\text{ }\mu\text{s}}$.

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

- **Tổng RAM tĩnh dự đoán (bytes), so với `W4E1_RAM_BUDGET_BYTES`**: **`11.904 / 15.684 bytes`** (chiếm **75,90%** ngân sách được giao, còn dư biên an toàn **`3.780 bytes`**; chi tiết từng hạng mục xem trong [REPORT.md](REPORT.md) và `W4E1_RAM_BUDGET_TABLE`).
- **Tổng CPU utilization dự đoán (%), so với `W4E1_CPU_BUDGET_PERCENT_MAX`**:
  - Ở `LO-Mode` danh định: **`18,14%`** (tính riêng 5 task) hoặc **`20,24%`** (cộng thêm $2,10\%$ nền ISR hệ thống `SysTick` + `TIM2` $= 21\ \mu s / 1\ ms$) trên trần **`76%`**.
  - Ở `HI-Stress / Worst-Case` (cả 5 task chạy ở $C_{HI}$): **`29,19%`** (5 task) hoặc **`31,29%`** (tổng cộng kèm nền ISR) **`/ 76%`** (dư biên an toàn CPU **`44,71%`**).

---

## Rủi ro đã lường trước

- **Nếu I2C1 bus-recovery (W2.1) xảy ra đúng lúc control loop tới hạn: dự đoán ảnh hưởng tới WCRT của `control`**:
  - **Không làm `control` bị chặn ($B_{control} = 0\text{ }\mu\text{s}$)** và WCRT của `control` chỉ tăng tối đa từ $70\text{ }\mu\text{s}$ lên **$97\text{ }\mu\text{s}$ ($\le 113\text{ }\mu\text{s}$)** nhờ 3 chốt thiết kế:
    1. Trình tự phát 9 xung SCL + START/STOP ($\approx 120\text{ }\mu\text{s}$) **chạy hoàn toàn trong ngữ cảnh task `imu` (Priority 7)** với ngắt được bật, tuyệt đối không chạy trong `I2C1_ER_IRQHandler` và không bọc trong `taskENTER_CRITICAL()`, nên `TIM3_IRQHandler` và task `control` (Priority 8) preempt `imu` ngay lập tức.
    2. Dữ liệu giữa `imu` và `control` truyền qua **bộ đệm SPSC / Seqlock không khóa** (kế thừa W2.2) thay vì dùng chung `I2C1 Mutex`, nên `control` đọc lại mẫu hợp lệ gần nhất và tăng `imu_stale_count` mà không chờ bus.
    3. Ngắt lỗi `I2C1_ER_IRQHandler` chỉ xóa cờ lỗi, tắt `ITEVFEN/ITERREN` và gọi `vTaskNotifyGiveFromISR()` trong $< 6\text{ }\mu\text{s}$.
- **Nếu mentor không duyệt thiết kế lần đầu: điểm nào trong bảng trên bạn nghĩ sẽ bị yêu cầu sửa, vì sao**:
  - **Điểm nhạy cảm nhất là xung đột giữa `STOP mode` (W3.2) và chu kỳ $1\text{ ms}$ (Jitter $\le 113\text{ }\mu\text{s}$) của `control`**: Dự báo của W3.2 (chưa đo tách riêng được vì chân PA0 chỉ lên mức cao sau khi PLL đã khóa) là thoát `STOP mode` và chờ thạch anh `HSE 8 MHz` + khóa lại `PLL 72 MHz` mất **$\approx 1,71\text{--}2,21\text{ ms}$**, đồng thời `STOP mode` tắt toàn bộ clock APB1/AHB (làm dừng `TIM3`, `TIM2` và `DMA1`). Nếu gọi `STOP mode` giữa hai chu kỳ $1\text{ ms}$ của `control` thì chắc chắn vỡ deadline. Do đó trong thiết kế này, chúng tôi đã chủ động chốt cơ chế **Adaptive Sleep Gating**: khi vòng điều khiển `control` 1 kHz đang active (hoặc đang truyền DMA), hook `portSUPPRESS_TICKS_AND_SLEEP` chỉ dùng lệnh `WFI` ở chế độ **`SLEEP mode`** (giữ nguyên HSE/PLL 72 MHz và clock APB, thời gian thức dậy $< 2\text{ }\mu\text{s}$); chỉ khi hệ thống được lệnh shell đưa vào trạng thái Standby/Idle dài hạn ($\ge 5\text{ ticks}$ không có task chu kỳ ngắn) thì mới hạ xuống **`STOP mode`** dùng RTC LSI.
