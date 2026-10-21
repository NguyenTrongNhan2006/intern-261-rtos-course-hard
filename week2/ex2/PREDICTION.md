# W2.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán đúng đắn

- Với W2E2_STRESS_COUNT = 1000000, dự đoán nhận đủ seq từ 0 đến 999999,
  không mất/lặp và không overrun nếu consumer theo kịp producer.
  Ring dùng đủ 512 slot; chỉ producer ghi head, chỉ consumer ghi tail.
  Load/store uint32_t căn chỉnh 4 byte là atomic trên Cortex-M3.
  __DMB() bảo đảm ghi payload trước khi công bố head, quan sát head trước
  khi đọc payload và đọc xong trước khi cập nhật tail để trả slot.
  volatile không thay thế memory barrier. Nếu push trả false do đầy,
  phải ghi nhận overrun, không được bỏ qua rồi kết luận PASS.

## Dự đoán hiệu năng

- Dự đoán cycle trung bình trên ring buffer: push khoảng 30 cycle,
  pop khoảng 30 cycle; tổng một cặp khoảng 60 cycle.

- Dự đoán cycle trung bình của Queue: xQueueSendFromISR khoảng 100 cycle,
  xQueueReceive khoảng 140 cycle; tổng một cặp khoảng 240 cycle.

- Dự đoán ring nhanh hơn khoảng 4 lần. Queue có thêm critical section
  dùng BASEPRI, quản lý trạng thái, memcpy tổng quát và kiểm tra waiting list.
  Ring cũng copy payload nhưng chỉ cần xử lý phần tử 4 byte và hai bộ đếm.
  Trong tổng 240 cycle của Queue, dự kiến khoảng 60 cho critical section,
  60 cho copy và cập nhật con trỏ, 20 cho kiểm tra waiting list rỗng,
  100 cho các kiểm tra và xử lý còn lại. Đây là phân bổ giả định để
  đối chiếu số đo, không phải chi phí cố định do tài liệu quy định.
  Khi không có task chờ, không tính thêm chi phí đánh thức hoặc chuyển task.

Các ước lượng trên giả định CPU 72 MHz, build -O2, cùng phần tử 4 byte
và W2E2_BENCH_ITERATIONS = 7048 lần thành công cho mỗi cơ chế.
Đo push/send trong ISR, pop/receive trong task; Queue receive không blocking.
Hiệu chỉnh chi phí đọc DWT, không tính thời gian chờ, log UART hay vào/ra ISR.
Kết quả thực tế cần đo xác nhận trước khi điền REPORT.md.
