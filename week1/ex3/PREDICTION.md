# W1.3 PREDICTION

MSSV: 2452882. Soạn trước khi nạp/chạy firmware W1E3.

## 1. Mô hình

Một CPU, bốn task periodic preemptive RM, D = T, không blocking.
RTA cơ sở bỏ qua overhead scheduler/tracer/ISR.

| Task | T (ms) | D (ms) | C (µs) | C (ms) | Priority |
| :--- | ---: | ---: | ---: | ---: | ---: |
| τ0 | 60 | 60 | 9909 | 9.909 | 4 (cao nhất) |
| τ1 | 100 | 100 | 10062 | 10.062 | 3 |
| τ2 | 200 | 200 | 31486 | 31.486 | 2 |
| τ3 | 400 | 400 | 74591 | 74.591 | 1 (thấp nhất) |

## 2. Utilization và Liu-Layland

U0 = 9.909/60 = 0.16515, U1 = 10.062/100 = 0.10062,
U2 = 31.486/200 = 0.15743, U3 = 74.591/400 = 0.1864775.

U = 0.6096775 (60.97%).

U_LL = 4(2^(1/4) − 1) = 0.756828460 (75.68%).

U < U_LL → schedulable (điều kiện đủ). Nếu U vượt LL thì chưa được
kết luận không schedulable — phải chạy RTA.

## 3. RTA (đơn vị ms)

Công thức: R_i^(n+1) = C_i + Σ_{j∈hp(i)} ⌈R_i^(n)/T_j⌉ · C_j.

**τ0:** R0 = 9.909, hội tụ ngay (không có task ưu tiên cao hơn).

**τ1:** R1^(0) = 10.062 → R1^(1) = 10.062 + ⌈10.062/60⌉·9.909
= 10.062 + 9.909 = 19.971 → R1^(2) = 19.971, hội tụ.

**τ2:** R2^(0) = 31.486 → R2^(1) = 31.486 + ⌈31.486/60⌉·9.909
+ ⌈31.486/100⌉·10.062 = 31.486 + 9.909 + 10.062 = 51.457
→ R2^(2) = 51.457, hội tụ.

**τ3:** R3^(0) = 74.591 → R3^(1) = 74.591 + 2·9.909 + 10.062
+ 31.486 = 135.957 → R3^(2) = 74.591 + 3·9.909 + 2·10.062
+ 31.486 = 155.928 → R3^(3) = 155.928, hội tụ.

| Task | RTA (ms) | D (ms) | Slack (ms) |
| :--- | ---: | ---: | ---: |
| τ0 | 9.909 | 60 | 50.091 |
| τ1 | 19.971 | 100 | 80.029 |
| τ2 | 51.457 | 200 | 148.543 |
| τ3 | 155.928 | 400 | 244.072 |

Tất cả R ≤ D. Task set gốc schedulable, không cần sửa T/C/priority.

## 4. Lịch critical instant (cùng release t = 0)

τ0: 0 → 9.909. τ1: 9.909 → 19.971. τ2: 19.971 → 51.457.
τ3: 51.457 → 60 (bị preempt), τ0: 60 → 69.909,
τ3: 69.909 → 100, τ1: 100 → 110.062,
τ3: 110.062 → 120, τ0: 120 → 129.909,
τ3: 129.909 → 155.928 hoàn tất. τ3 bị preempt 3 lần.

## 5. Kế hoạch đo

H = LCM(60,100,200,400) = 1200 ms. Đo trong 5H = 6000 ms.
Job dự kiến: τ0 = 100, τ1 = 60, τ2 = 30, τ3 = 15, tổng 205.
Epoch chung tại tick 20. C quy đổi: 713448, 724464, 2266992,
5370552 cycles (72 MHz).

WCRT thực dự kiến lớn hơn RTA danh định do overhead SysTick ISR,
PendSV context switch và tracer hooks. Các overhead này cần đo
trực tiếp trên firmware W1E3 để định lượng chênh lệch.
