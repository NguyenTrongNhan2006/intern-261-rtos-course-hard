# W4.2 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán priority/stack
- Bạn dự đoán task nào sẽ có stack high-water-mark gần ngân sách nhất (margin nhỏ nhất): ___, lý do: ___.
- Dự đoán CPU load tổng khi hệ thống chạy ổn định (không fault): ___ %, so với `W4E1_CPU_BUDGET_PERCENT_MAX`.

## Dự đoán hành vi tickless
- Dự đoán hệ thống có vào STOP mode được không khi IMU/telemetry/shell đều rảnh: ___.
- Dự đoán độ trễ thức dậy từ STOP mode có ảnh hưởng tới jitter control loop không, bao nhiêu us: ___.

## Rủi ro đã lường trước
- Nếu priority đã duyệt ở W4.1 bị vi phạm khi hiện thực (do nhầm lẫn): dự đoán triệu chứng quan sát được: ___.
- Nếu `imu` không kịp cung cấp mẫu mới trước khi `control` cần: dự đoán hệ quả (dùng mẫu cũ, bỏ chu kỳ, hay gì khác): ___.
