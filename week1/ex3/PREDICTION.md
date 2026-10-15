# W1.3 PREDICTION

MSSV: 2452882. Soạn ngày 16/10/2026, trước khi nạp/chạy firmware W1E3.

## 1. Mô hình và bộ tham số gốc

Một CPU, bốn task độc lập, periodic, preemptive fixed priority, D = T,
không mutex/blocking và không self-suspension trong phần việc C.
RTA cơ sở bỏ qua overhead scheduler, tracer và ISR. Các overhead được đo riêng.

| Task | T (ms) | D (ms) | C (us) | C (ms) | Priority FreeRTOS |
| :--- | ---: | ---: | ---: | ---: | ---: |
| tau0 | 60 | 60 | 9909 | 9.909 | 4 |
| tau1 | 100 | 100 | 10062 | 10.062 | 3 |
| tau2 | 200 | 200 | 31486 | 31.486 | 2 |
| tau3 | 400 | 400 | 74591 | 74.591 | 1 |

Priority lớn hơn là ưu tiên cao hơn. Idle dùng priority 0.

## 2. Utilization và Liu-Layland

U0 = 9.909 / 60 = 0.16515 = 16.515%.

U1 = 10.062 / 100 = 0.10062 = 10.062%.

U2 = 31.486 / 200 = 0.15743 = 15.743%.

U3 = 74.591 / 400 = 0.1864775 = 18.64775%.

U = 0.6096775 = 60.96775%.

U_LL = 4 * (2^(1/4) - 1) = 0.756828460 = 75.682846%.

U < U_LL: task set schedulable trong mô hình lý tưởng. LL là điều kiện đủ;
nếu vượt bound thì chưa được kết luận không schedulable. Nếu U > 1 thì
không thể đáp ứng toàn bộ workload lâu dài trên một CPU.

## 3. RTA, mọi phép tính bên dưới dùng ms

R_i^(0) = C_i.

R_i^(n+1) = C_i + sum(j có priority cao hơn i) ceil(R_i^n / T_j) * C_j.

ceil(x) là làm tròn lên: ceil(74.591 / 60) = 2.

tau0: R0^(0) = 9.909; R0^(1) = 9.909, hội tụ.

tau1: R1^(0) = 10.062.

R1^(1) = 10.062 + ceil(10.062 / 60) * 9.909 = 19.971.

R1^(2) = 10.062 + ceil(19.971 / 60) * 9.909 = 19.971, hội tụ.

tau2: R2^(0) = 31.486.

R2^(1) = 31.486 + ceil(31.486 / 60) * 9.909 + ceil(31.486 / 100) * 10.062
= 51.457.

R2^(2) = 31.486 + ceil(51.457 / 60) * 9.909 + ceil(51.457 / 100) * 10.062
= 51.457, hội tụ.

tau3: R3^(0) = 74.591.

R3^(1) = 74.591 + ceil(74.591 / 60) * 9.909
+ ceil(74.591 / 100) * 10.062 + ceil(74.591 / 200) * 31.486
= 74.591 + 2 * 9.909 + 10.062 + 31.486 = 135.957.

R3^(2) = 74.591 + ceil(135.957 / 60) * 9.909
+ ceil(135.957 / 100) * 10.062 + ceil(135.957 / 200) * 31.486
= 74.591 + 3 * 9.909 + 2 * 10.062 + 31.486 = 155.928.

R3^(3) = 74.591 + ceil(155.928 / 60) * 9.909
+ ceil(155.928 / 100) * 10.062 + ceil(155.928 / 200) * 31.486
= 155.928, hội tụ.

| Task | RTA (ms) | D (ms) | D - R (ms) | Kết luận |
| :--- | ---: | ---: | ---: | :--- |
| tau0 | 9.909 | 60 | 50.091 | Đạt |
| tau1 | 19.971 | 100 | 80.029 | Đạt |
| tau2 | 51.457 | 200 | 148.543 | Đạt |
| tau3 | 155.928 | 400 | 244.072 | Đạt |

Task set gốc schedulable theo RM trong mô hình đã nêu. Không cần sửa T/C/priority.

## 4. Lịch dự kiến tại critical instant

Cùng release tại t = 0.

tau0 chạy 0 đến 9.909 ms.

tau1 chạy 9.909 đến 19.971 ms.

tau2 chạy 19.971 đến 51.457 ms.

tau3 chạy 51.457 đến 60 ms, còn 66.048 ms CPU.

tau0 chạy 60 đến 69.909 ms.

tau3 chạy 69.909 đến 100 ms, còn 35.957 ms CPU.

tau1 chạy 100 đến 110.062 ms.

tau3 chạy 110.062 đến 120 ms, còn 26.019 ms CPU.

tau0 chạy 120 đến 129.909 ms.

tau3 chạy 129.909 đến 155.928 ms, hoàn tất job đầu.

tau3 bị preempt 3 lần trong job này; số preemption không đồng nghĩa toàn bộ số context switch.

## 5. Dự đoán và kế hoạch đo

H = LCM(60, 100, 200, 400) = 1200 ms. Đo release trong 5H = 6000 ms.
Số job dự kiến: tau0 = 100, tau1 = 60, tau2 = 30, tau3 = 15, tổng 205.
Toàn bộ release dùng cùng epoch ở tick 20. Không lấy lần task bắt đầu chạy
làm release. Response = completion_timestamp - nominal_release_timestamp.

72 MHz tương ứng 72 cycles/us. C quy đổi lần lượt thành 713448,
724464, 2266992 và 5370552 cycles.

REPORT W1E2 hiện có ghi baseline d673d5f: OUT->IN tại O2 = 264..290 cycles
= 3.666667..4.027778 us, mean 278.214 cycles = 3.864083 us.
Các số trên thuộc baseline cũ, chỉ dùng làm tham chiếu trước thí nghiệm;
tracer hiện tại và wrapper W1E3 phải đo lại. OUT->IN không bao phủ toàn bộ
exception entry/exit, PendSV save/restore hoặc tick ISR.

Lịch lý tưởng có số lần đổi task bên trong cửa sổ response đầu tiên của
tau0/tau1/tau2/tau3 lần lượt 0/1/2/9 (không tính dispatch vào tau0 từ Idle).
Nếu tạm dùng mean baseline cho riêng phần khoảng OUT->IN, đóng góp minh họa
là 0/3.864083/7.728167/34.776750 us. Đây không phải tổng overhead hoặc
dự đoán chính xác WCRT phần cứng. Firmware W1E3 có thêm wrapper accounting.

Dự kiến observed WCRT ở gần các RTA trên. Overhead thường làm tăng latency;
busy-wait theo selected-time vẫn tính một phần ISR/exception vào C, nên
chênh lệch thực có thể bị bù và không bắt buộc dương ở mọi mẫu.
Selected-time loại thời gian task khác chạy, nhưng chưa phải exclusive
thread CPU time. Cần hiệu chỉnh, đo overshoot, thời gian tick ISR và
timestamp offset trước khi kết luận độ chính xác workload.

DeltaR = observed_R - ideal_R. Đối chiếu phần N_switch * measured_OUT_IN,
cộng phần ISR/PendSV/hook chưa phủ, release-grid error và budget calibration.
Không coi N_preemption * 278.214 cycles là lời giải đầy đủ.

Epoch lấy trong tick hook; lưới release danh định có sai số timestamp do
vị trí hook, SysTick latency và clock. Đối chiếu GPIO/LA nếu cần định lượng
ở mức us. WCRT trên 205 job là maximum quan sát hữu hạn, không chứng minh
worst case tuyệt đối của phần cứng.

## 6. Trạng thái

Đây là dự đoán trước khi chạy W1E3. Build/flash/đo W1E3: [CẦN BỔ SUNG].
Commit file này trước khi thêm code và nạp firmware. params.h và PARAMS.md
được repo ignore; bộ tham số đã được ghi đầy đủ trong prediction để lưu vết.

Ghi chú bổ sung sau các phép đo: bản dự đoán này được soạn trong deliverables
ở bước chuẩn bị, nhưng PREDICTION.md trong repo vẫn là template khi kiểm tra
sau capture. Chưa có bằng chứng commit prediction W1E3 trước khi chạy.
Nếu đưa bản này vào repo lúc này, phải ghi nhận đó là commit sau phép đo;
không cập nhật các dự đoán trên thành số đo để tạo vẻ dự đoán chính xác.
