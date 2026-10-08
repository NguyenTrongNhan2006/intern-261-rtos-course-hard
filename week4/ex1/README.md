# W4.1 – Thiết kế hệ thống mixed-criticality

## Mục tiêu
Thiết kế TRÊN GIẤY (không viết code thân driver/task) một hệ thống mixed-criticality: control loop 1 kHz
kích bằng TIM (jitter tối đa theo MSSV), IMU 500 Hz qua driver I2C W2.1, telemetry UART DMA, shell lệnh,
tickless khi rảnh — và chứng minh bằng Response Time Analysis (RTA) rằng tập task này schedulable TRƯỚC khi
viết bất kỳ dòng code nào của W4.2.

## Đọc trước
- Tổng hợp toàn bộ tuần 1–3 của chính bạn: W1.3 (RTA/Liu–Layland), W1.4 (ngân sách RAM tĩnh), W2.1 (I2C1),
  W3.1 (UART DMA), W3.2 (tickless idle).
- Mastering FreeRTOS – chương Scheduling (fixed-priority, Rate Monotonic) và chương Low Power (tickless).
- RM0008: chương TIM (so sánh/kích ngắt chu kỳ), chương I2C, chương USART/DMA (ôn lại, không đọc lại từ đầu).

## Việc phải làm
1. Liệt kê toàn bộ task cần thiết (tối thiểu 5 vai trò: control, imu, telemetry, shell, idle_aux) với
   priority đề xuất, period/kích hoạt, WCET ước lượng (dựa trên số đo thật đã có từ tuần 1–3, không đoán mò).
2. Vẽ task diagram: ai gọi ai, ai dùng tài nguyên dùng chung nào (I2C1, UART DMA, biến trạng thái shell),
   cơ chế đồng bộ nào (queue/notification/event group) cho từng cạnh.
3. Chạy RTA bằng tay cho toàn bộ tập task (không chỉ Liu–Layland, vì đó chỉ là điều kiện đủ) với WCET đã ước
   lượng ở bước 1; chứng minh R_i ≤ D_i cho MỌI task, kể cả khi I2C1 bus-recovery (W2.1) hoặc ISR UART
   (W3.1) chiếm CPU ở tình huống xấu nhất.
4. Lập bảng ngân sách RAM tĩnh (mọi task stack + mọi buffer tĩnh) và so với `W4E1_RAM_BUDGET_BYTES`
   (xem `PARAMS.md`); lập ước lượng CPU utilization tổng và so với `W4E1_CPU_BUDGET_PERCENT_MAX`.
5. Lập bảng failure mode: với mỗi tài nguyên dùng chung (I2C1, UART, bộ nhớ), nếu nó hỏng/treo thì hệ thống
   phải xử lý ra sao (ai phát hiện, phát hiện bằng gì, phản ứng gì) — đây là nền cho `faults.md` ở W4.3.
6. Điền `include/mixed_criticality_design.h` (chỉ khai báo kiểu dữ liệu/bảng, KHÔNG thân hàm) làm bảng tổng
   kết RTA chính thức, dùng lại nguyên văn ở W4.2/W4.3.
7. Nộp design doc cho mentor duyệt; CHỈ bắt đầu viết code W4.2 sau khi được duyệt.

## Sản phẩm nộp
- Design doc (có thể là phần mở rộng của README/REPORT của chính bạn): task diagram, bảng RTA đầy đủ (T, D,
  C, R cho mọi task), bảng ngân sách RAM/CPU, bảng failure mode.
- `include/mixed_criticality_design.h` điền đầy đủ theo kiểu đã khai báo.

## Tiêu chí đạt
RTA chứng minh schedulable trên giấy (mọi R_i ≤ D_i) TRƯỚC khi viết code; ngân sách RAM/CPU nằm trong giới
hạn `PARAMS.md`; bảng failure mode đủ chi tiết để mentor duyệt không cần hỏi lại.

## Giờ dự kiến
4 giờ.
