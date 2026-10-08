# W2.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán đúng đắn
- Với `W2E2_STRESS_COUNT` phần tử (xem PARAMS.md), dự đoán: ring buffer tự viết có mất/lặp phần tử nào không? Vì
  sao (dựa trên memory barrier bạn đặt): ___.

## Dự đoán hiệu năng
- Dự đoán cycle trung bình 1 lần push/pop trên ring buffer tự viết: ___ cycle.
- Dự đoán cycle trung bình 1 lần `xQueueSendFromISR` + `xQueueReceive`: ___ cycle.
- Dự đoán ring buffer nhanh hơn/chậm hơn queue gốc bao nhiêu lần, và vì sao (copy, critical section, wake task):
  ___.
