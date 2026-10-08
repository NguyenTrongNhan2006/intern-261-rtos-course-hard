# W2.4 – REPORT

## Số đo
- Bảng RAM/cycle trước (binary semaphore) – sau (notification đã sửa bug):

| Cơ chế | RAM tĩnh (bytes) | Cycle trung bình 1 lần báo hiệu |
|---|---|---|
| Binary semaphore (trước) | | |
| Notification đã sửa (sau) | | |

- Báo cáo bug (BẮT BUỘC có số liệu từ tracer, không chỉ mô tả lý thuyết):

| Biến thể | Số lần gửi | Số sự kiện nhận được | Chênh lệch (mất) |
|---|---|---|---|
| `eSetValueWithOverwrite` (buggy) | | | |
| Phiên bản đã sửa | | | |

## Giải thích sai lệch (ĐỊNH LƯỢNG)
- So dự đoán số sự kiện mất trong PREDICTION.md với số đo thực; giải thích bằng đúng tốc độ gửi/nhận đo được
  (không phải "do race condition" chung chung).
- Giải thích cơ chế chính xác: tại thời điểm nào 2 lần gọi `xTaskNotify(..., eSetValueWithOverwrite)` liên tiếp dẫn
  tới việc `xTaskNotifyWait` phía nhận chỉ thấy giá trị thứ hai.
- Giải thích vì sao cơ chế bạn chọn để sửa (`eSetBits` tích luỹ / `eIncrement` + đếm) không thể mất sự kiện trong
  cùng kịch bản tốc độ gửi/nhận đó.

## Trả lời câu hỏi trade-off
RAM/cycle của notification so với binary semaphore: đổi lấy gì để được gì? Nếu tốc độ gửi tăng vượt xa tốc độ
nhận, cơ chế đã sửa của bạn có còn đúng (không mất dữ liệu) hay chỉ KHÔNG mất SỰ KIỆN (số lần) nhưng có thể mất dữ
liệu đi kèm? Chứng minh bằng số đo của bạn, không suy đoán.
