# W3.2 – REPORT

## Số đo
- Bảng dòng tiêu thụ:

| Trạng thái | Dòng đo (mA / µA) |
|---|---|
| Active | |
| Idle (STOP mode) | |

- Bảng trôi tick trong `W3E2_DRIFT_TEST_WINDOW_MIN` phút:

| Giờ MCU | Giờ PC | Chênh lệch (ms) |
|---|---|---|
| | | |

- Ảnh logic analyzer đối chiếu thời điểm vào/ra STOP (chân toggle tự thêm).

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- So trôi tick đo được với ngưỡng `W3E2_DRIFT_BOUND_PPM` (PARAMS.md): đạt/không đạt.
- Phân tách nguồn trôi: dung sai oscillator RTC (ppm danh định) chiếm bao nhiêu, làm tròn khi bù tick
  (`vTaskStepTick`) chiếm bao nhiêu, thời gian entry/exit STOP (đo bằng tracer/logic analyzer) chiếm bao nhiêu –
  cộng lại phải khớp (hoặc giải thích phần không khớp) với tổng trôi đo được.
- So dự đoán dòng tiêu thụ trong PREDICTION.md với số đo thực tế; giải thích chênh lệch.

## Trả lời câu hỏi trade-off
Nếu giảm tần số đánh thức RTC (ngủ lâu hơn mỗi lần) để tiết kiệm điện hơn nữa, điều gì đánh đổi về độ chính xác
bù tick và về khả năng phản ứng với các task khác? Chứng minh bằng số đo của chính bạn.
