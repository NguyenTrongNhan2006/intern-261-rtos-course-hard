# W2.1 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán trình tự ngắt bình thường

- Số lần ngắt EV cho transaction write N byte: khoảng N + 3 lần (SB, ADDR, N lần TXE, BTF cuối), nếu các sự kiện được xử lý riêng.
- Thời gian từ yêu cầu START đến xử lý EV5: dự kiến vài µs khi bus rảnh, không bị ngắt ưu tiên cao trì hoãn; kiểm chứng bằng DWT.

## Dự đoán bus recovery

- Số xung SCL trong trường hợp xấu nhất: 9 xung; không bảo đảm thành công nếu SDA vẫn bị giữ LOW từ bên ngoài.
- Dự đoán phục hồi đủ 11 lần liên tiếp: có, nếu tác động gây lỗi được gỡ và slave hoạt động lại bình thường.
- Thời gian từ phát hiện bus-stuck đến transaction tiếp theo thành công: dự kiến 1–5 ms nếu lỗi đã được gỡ, giao dịch ngắn và task được chạy ngay; cần đo xác nhận.

## Dự đoán timeout

- Timeout cấu hình: 28 ms. Chưa cấu hình watchdog cho Ex1. Nếu có watchdog, nó có thể can thiệp trước khi task xử lý timeout khi thời gian watchdog còn lại quá ngắn hoặc task bị trì hoãn.
