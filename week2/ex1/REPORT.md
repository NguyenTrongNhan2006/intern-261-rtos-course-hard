# W2.1 – REPORT

## Số đo
- Ảnh logic analyzer: trình tự EV5–EV8 bình thường (1 transaction write + 1 transaction read).
- Ảnh logic analyzer: trình tự bus recovery (rút dây SDA giữa transaction → toggle SCL → START/STOP → phục hồi).
- Bảng số lần phục hồi liên tiếp:

| Lần thử | Kết quả (OK / timeout / recovered) | Số xung SCL cần toggle |
|---|---|---|
| | | |

Tổng số lần phục hồi liên tiếp thành công: ___ / `W2E1_RECOVERY_REPEAT_COUNT` (PARAMS.md).

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- So sánh số xung SCL dự đoán trong PREDICTION.md với số đo thực tế; giải thích chênh lệch (nếu có) bằng trạng thái
  nội bộ của slave MPU6050 tại thời điểm rút dây.
- So sánh thời gian phục hồi đo được với dự đoán; nguồn sai số nào (polling GPIO, overhead ISR, timeout) ảnh hưởng
  nhiều nhất?
- Nếu có lần không phục hồi được trong `W2E1_RECOVERY_REPEAT_COUNT` lần: định vị nguyên nhân bằng tracer W1.2 + ảnh
  logic analyzer, không đoán.

## Trả lời câu hỏi trade-off
Vì sao bus recovery phải chuyển SCL/SDA sang GPIO thường thay vì tiếp tục dùng thanh ghi I2C để tạo xung? Nếu bỏ
qua bước phát STOP sau khi toggle SCL, điều gì sẽ xảy ra với transaction tiếp theo? Chứng minh bằng số đo của bạn.
