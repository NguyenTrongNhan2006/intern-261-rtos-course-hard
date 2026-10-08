# W4.4 – PREDICTION (điền TRƯỚC buổi bảo vệ, commit trước)

## Tóm tắt số liệu mang theo (điền từ W4.1-W4.3)
- RAM: ___ / `W4E1_RAM_BUDGET_BYTES` bytes. CPU: ___ / `W4E1_CPU_BUDGET_PERCENT_MAX` %.
- Jitter control loop đo được xấu nhất: ___ us / `W4E1_CONTROL_LOOP_JITTER_MAX_US` us.
- Kịch bản fault có margin suy giảm MỎNG NHẤT (gần vi phạm deadline nhất dù đã kiểm soát): ___.

## Dự đoán phản ứng khi mentor đổi tham số tại chỗ
- Nhóm tham số có thể bị đổi (xem `W4E4_MUTATION_TARGET_HINT`): ___.
- Nếu giá trị trong nhóm đó TĂNG: dự đoán hướng ảnh hưởng tới WCRT/jitter/RAM (tăng/giảm, không cần số chính
  xác): ___.
- Nếu giá trị trong nhóm đó GIẢM: dự đoán hướng ảnh hưởng: ___.

## Chiến lược chẩn đoán lỗi chưa biết trước
- Công cụ đo nào bạn sẽ dùng ĐẦU TIÊN khi thấy triệu chứng lạ (tracer? run-time stats? GDB? `.noinit`?), theo
  thứ tự ưu tiên: ___.
