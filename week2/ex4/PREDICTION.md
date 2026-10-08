# W2.4 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán barrier
- Dự đoán thời gian (us/cycle) từ khi task CHẬM NHẤT gọi `xEventGroupSync` tới khi CẢ `W2E4_NUM_TASKS` task được
  giải phóng cùng lúc: ___.

## Dự đoán bug mất sự kiện
- Dự đoán: với tốc độ gửi/nhận trong code của bạn, có xảy ra mất sự kiện khi dùng `eSetValueWithOverwrite` không?
  Vì sao (so tốc độ gửi vs tốc độ task nhận xử lý): ___.
- Dự đoán số sự kiện bị mất trên 1000 lần gửi (nếu có): ___.

## Dự đoán sau khi sửa
- Dự đoán sau khi đổi cơ chế: số gửi có luôn bằng số nhận không trong mọi tốc độ gửi? Vì sao: ___.

## Dự đoán RAM/cycle
- Dự đoán RAM tĩnh event group so với bản binary semaphore cũ: tăng/giảm bao nhiêu byte, vì sao: ___.
- Dự đoán cycle trung bình đường notification so với binary semaphore: ___.
