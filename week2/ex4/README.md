# W2.4 – Event group, barrier, notification

## Mục tiêu
Dựng barrier N task bằng `xEventGroupSync`, thay toàn bộ các chỗ dùng binary semaphore bằng task notification, rồi
tự phát hiện (bằng tracer, không đoán) một bug mất sự kiện gây ra bởi `eSetValueWithOverwrite`.

## Đọc trước
- Mastering ch.9 (Event Group) – đọc kỹ `xEventGroupSync` (rendezvous/barrier).
- Mastering ch.10 (Task Notification) – đọc kỹ sự khác nhau giữa `eSetBits`, `eSetValueWithOverwrite`,
  `eSetValueWithoutOverwrite`, `eIncrement`.

## Việc phải làm
1. `W2E4_NUM_TASKS` task (params.h) đồng bộ tại 1 điểm bằng `xEventGroupSync`, mỗi task dùng 1 bit riêng, chờ đủ
   `W2E4_SYNC_BITS_MASK`, timeout `W2E4_SYNC_TIMEOUT_MS`.
2. Viết lại phiên bản đầu tiên của hệ thống đồng bộ (các chỗ trước đây dùng binary semaphore để báo "có 1 sự kiện
   mới") bằng `xTaskNotify`/`xTaskNotifyWait` dùng `eSetValueWithOverwrite` ở phía gửi — đây CHÍNH LÀ bản có bug cài
   sẵn: nếu người gửi gọi 2 lần trước khi người nhận kịp `xTaskNotifyWait` 1 lần, giá trị cũ bị ghi đè mất, người
   nhận chỉ thấy 1 sự kiện dù có 2.
3. Dùng tracer W1.2 (ghi lại mọi lần gửi/nhận notify kèm giá trị + timestamp) để tự phát hiện: số lần gửi phải
   bằng số lần nhận cộng dồn; nếu lệch, đó là bằng chứng mất sự kiện.
4. Sửa lại bằng cơ chế không mất sự kiện (ví dụ `eSetBits` tích luỹ bit, hoặc `eIncrement` + so sánh đếm, tự chọn
   và giải thích lựa chọn) và chứng minh lại bằng tracer rằng số gửi = số nhận.
5. Đo RAM tĩnh (kích thước event group + TCB liên quan) và cycle trung bình của đường đồng bộ TRƯỚC (binary
   semaphore, dữ liệu lấy lại từ bài cũ nếu có, hoặc đo bằng dựng lại nhanh) và SAU (notification) khi đã sửa bug.

## Sản phẩm nộp
- Code barrier (`xEventGroupSync`) + phiên bản notification có bug + bản đã sửa.
- Bảng RAM/cycle trước–sau.
- Báo cáo bug (số gửi vs số nhận, bằng chứng từ tracer, KHÔNG được chỉ nói "em đoán là do overwrite").

## Tiêu chí đạt
Tìm được bug BẰNG TRACER (có số liệu gửi/nhận cụ thể), không đoán; giải thích đúng cơ chế `eSetValueWithOverwrite`
gây mất sự kiện.

## Giờ dự kiến
4 giờ.
