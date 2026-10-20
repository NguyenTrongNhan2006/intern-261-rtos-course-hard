# W1.3 REPORT: RM, RTA và đo WCRT trên STM32

MSSV: 2452882. STM32F103 Cortex-M3, HCLK 72 MHz.
Dữ liệu: [measurements/](measurements/README.md).

## 1. Mô hình lý thuyết

Bốn task periodic preemptive RM, D = T, không blocking.
Utilization, Liu-Layland bound và bước lặp RTA: xem
[PREDICTION.md](PREDICTION.md).

| Task | T = D (ms) | C (µs) | Priority | U_i | RTA (ms) |
| :--- | ---: | ---: | ---: | ---: | ---: |
| τ0 | 60 | 9909 | 4 | 0.16515 | 9.909 |
| τ1 | 100 | 10062 | 3 | 0.10062 | 19.971 |
| τ2 | 200 | 31486 | 2 | 0.15743 | 51.457 |
| τ3 | 400 | 74591 | 1 | 0.18648 | 155.928 |

U = 0.6097 < U_LL = 4(2^(1/4)−1) = 0.7568. Tất cả R ≤ D →
schedulable trong mô hình không overhead.

## 2. Phương pháp đo

H = LCM(60,100,200,400) = 1200 ms. Epoch tick 20, đo 5H = 6000 ms.
Mỗi capture: 205 job (100/60/30/15). Response = finish − release
(DWT timestamp trong tick hook và job_end).

V1 dùng selected-time busy-wait (credit ISR vào C). V2 chạy đủ vòng
SUBS/BNE cố định đã hiệu chỉnh DWT, preemption không rút ngắn số
vòng; bọc SysTick/PendSV bằng wrapper assembly đo bracket trực tiếp.
Xem [PREDICTION_V2.md](PREDICTION_V2.md).

Build V2: Arm GCC 14.2.1, -Wall -Wextra -Werror. O0 Flash 14212 B,
O2 Flash 8808 B, heap còn 3056 B. API check PASS. Prediction commit
59a16a7 trước flash V2.

## 3. V1 baseline

| Task | RTA (ms) | WCRT O0 (ms) | Δ O0 (µs) | WCRT O2 (ms) | Δ O2 (µs) |
| :--- | ---: | ---: | ---: | ---: | ---: |
| τ0 | 9.909 | 9.935 | 25.6 | 9.925 | 15.7 |
| τ1 | 19.971 | 20.033 | 62.5 | 20.007 | 36.4 |
| τ2 | 51.457 | 51.570 | 113.1 | 51.522 | 64.9 |
| τ3 | 155.928 | 156.243 | 315.2 | 156.103 | 175.0 |

Delta nhỏ vì selected-time accounting credit ISR vào C. Không áp
số liệu V1 cho firmware V2. Source V1 checkpoint a624a01; SHA256
trong measurements/BASELINE_V1_MANIFEST.json.

## 4. V2: Hiệu chỉnh DWT

| Task | C target (µs) | O0 C_cal (cycles) | O0 +Δ (µs) | O2 C_cal (cycles) | O2 +Δ (µs) |
| :--- | ---: | ---: | ---: | ---: | ---: |
| τ0 | 9909 | 713450 | +0.028 | 713453 | +0.069 |
| τ1 | 10062 | 724466 | +0.028 | 724469 | +0.069 |
| τ2 | 31486 | 2266994 | +0.028 | 2266997 | +0.069 |
| τ3 | 74591 | 5370554 | +0.028 | 5370557 | +0.069 |

8 mẫu/task, min = max, target ≤ C_cal ≤ target + 1 µs.
Calibration trước scheduler, IRQ tắt.

## 5. V2: WCRT quan sát

| Task | RTA (ms) | WCRT O0 (ms) | WCRT O2 (ms) | Misses |
| :--- | ---: | ---: | ---: | :--- |
| τ0 | 9.909 | 10.065 | 10.009 | 0 / 0 |
| τ1 | 19.971 | 20.284 | 20.172 | 0 / 0 |
| τ2 | 51.457 | 52.222 | 51.944 | 0 / 0 |
| τ3 | 155.928 | 158.209 | 157.401 | 0 / 0 |

100/60/30/15 job mỗi bản. Zero deadline miss. Release grid error:
O0 trong −9.6..+0.4 µs, O2 trong −3.2..0.0 µs.

## 6. V2: Phân tách overhead worst-case

Đồng nhất thức: Δ R = SysTick bracket + PendSV bracket +
(C_cal − C_nominal) + residual.

| Bản | Task | Δ R (µs) | SysTick (µs) | PendSV (µs) | Residual (µs) |
| :--- | :--- | ---: | ---: | ---: | ---: |
| O0 | τ0 | 155.958 | 103.431 | 15.431 | 37.069 |
| O0 | τ1 | 313.125 | 197.319 | 32.208 | 83.542 |
| O0 | τ2 | 764.819 | 501.917 | 66.681 | 196.139 |
| O0 | τ3 | 2281.444 | 1513.069 | 187.708 | 580.472 |
| O2 | τ0 | 99.889 | 72.042 | 8.792 | 18.986 |
| O2 | τ1 | 201.389 | 139.125 | 18.069 | 44.056 |
| O2 | τ2 | 487.458 | 348.889 | 37.111 | 101.250 |
| O2 | τ3 | 1473.444 | 1066.542 | 103.819 | 302.597 |

C_cal overshoot cho τ3: O0 = 0.194 µs (14 cycles), O2 = 0.486 µs
(35 cycles); đã trừ khỏi residual. Complete work counts τ3:
(3,2,1,1) đúng lịch critical-instant RTA. Empty bracket probe:
O0 = 70 cycles, O2 = 24–25 cycles.

PendSV bracket bao gồm tracer hook, scheduler, software save/restore.
Residual gồm hardware exception entry/exit, tail chaining, mép
wrapper và code đầu/cuối job. Chu kỳ gốc trong
measurements/results_v2_O0.json và results_v2_O2.json.

### So sánh O0 và O2

Toàn experiment: 5970 SysTick, 349 PendSV ở cả hai bản. SysTick
trung bình O0 = 688 cycles (9.56 µs), O2 = 488 cycles (6.78 µs),
giảm 29%. PendSV trung bình O0 = 805 cycles (11.18 µs), O2 = 472
cycles (6.56 µs), giảm 41%.

Chênh WCRT τ3 giữa O0 và O2 = 808.0 µs. Trong đó SysTick bracket
giải thích 446.5 µs, PendSV bracket 83.9 µs, residual 277.6 µs.
Toàn bộ chênh lệch phản ánh throughput mã máy khác nhau: -O0 sinh
code dài hơn 61% (14212 vs 8808 B), nhiều load/store trên stack,
pipeline utilization thấp hơn trên Cortex-M3.

## 7. Logic Analyzer

| Bản | Events | Max mismatch (µs) |
| :--- | ---: | ---: |
| V1 O0 | 254 | 7.819 |
| V1 O2 | 254 | 8.972 |
| V2 O0 | 254 | 9.306 |
| V2 O2 | 254 | 7.500 |

PA4..PA8 = Idle/τ0..τ3, 1 MHz. Tất cả dưới ngưỡng 16 µs. UART/LA
thu lượt reset riêng. JSON: measurements/correlation_v2_*.json.
Gantt: [O0](measurements/gantt_v2_O0.svg), [O2](measurements/gantt_v2_O2.svg).

## 8. Kết luận

LL xác nhận U < U_LL (điều kiện đủ). RTA cho R ≤ D ở cả bốn task
(cần và đủ trong mô hình không overhead). V2 đo 205 job × 2 bản,
zero deadline miss. Sai lệch WCRT − RTA phân tách định lượng bằng
SysTick/PendSV bracket và residual trong từng response window.

Số đo là maximum hữu hạn trong capture; profiler có observer effect,
chưa chứng minh WCRT tuyệt đối hoặc hiệu chuẩn oscillator.
SHA256: measurements/V2_MANIFEST.json.
