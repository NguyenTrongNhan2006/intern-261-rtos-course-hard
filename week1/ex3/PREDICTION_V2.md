# W1E3 V2: dự đoán trước phép đo mới

MSSV 2452882. Soạn sau khi xem kết quả V1, sau khi build/host-test
source V2, trước khi flash/đo V2 trên STM32. Không phải dự đoán mù.

## 1. Thay đổi so với V1

V2 chạy đủ vòng SUBS/BNE cố định đã hiệu chỉnh DWT trước scheduler
(IRQ tắt). Preemption không rút ngắn số vòng. SysTick/PendSV được bọc
bằng wrapper assembly riêng, đo bracket enter/exit bằng DWT.

## 2. Dự đoán calibration và RTA

C mục tiêu giữ nguyên 9909/10062/31486/74591 µs. Calibrate binary
search 8 mẫu; dự kiến min ≥ target, max ≤ target + 72 cycles (1 µs).

U = 0.6096775 < U_LL = 0.756828460. RTA danh định giữ nguyên:
9.909 / 19.971 / 51.457 / 155.928 ms. Nếu dùng C + 1 µs, RTA thành
9.910 / 19.973 / 51.460 / 155.935 ms — các phép ⌈⌉ chưa đổi.

## 3. Phép đo dự kiến

Release timestamp trong tick hook, chung cho các task release cùng
tick. 205 JOB2 trong 5H = 6000 ms. Mỗi job lưu response, work wall
time, SysTick/PendSV cycle count trong response window.

Delta = observed R − ideal RTA. Đối chiếu bằng SysTick bracket,
PendSV bracket và residual ngoài bracket. PendSV bracket bao gồm
tracer/scheduler/save-restore; không cộng OUT→IN thêm lần nữa.

## 4. Tên file

V1: trace_O0/O2.csv, capture_O0/O2.sr (giữ nguyên).
V2: trace_v2_O0/O2.csv, capture_v2_O0/O2.sr, build w1e3-v2-O0/O2.
