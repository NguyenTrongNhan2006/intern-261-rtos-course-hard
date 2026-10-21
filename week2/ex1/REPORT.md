# W2.1 – REPORT

MSSV: 2452882. STM32F103C8T6, I2C1 PB6/PB7 cấu hình 100 kHz, MPU6050 địa chỉ 0x68, timeout 28 ms; yêu cầu 11 lần phục hồi liên tiếp. Firmware đã chạy trên board và xuất UART, nhưng khởi tạo cảm biến chưa thành công.

## Số đo

Trích log UART từ ảnh chụp kết quả trên board:

```text
INIT,BUS,28125,0,0,,,,,,
INIT,BUS,28053,0,0,,,,,,
```

Cột thời gian có đơn vị µs: các lượt quan sát được mất khoảng 28,053–28,125 ms và trả BUS. Chưa có mẫu ACCEL/GYRO hợp lệ. Hai số 0 là RECOVERY_STREAK và RECOVERY_PENDING; các bộ đếm này chỉ cập nhật trong nhánh đọc mẫu, nên log INIT không chứng minh driver chưa từng thử recovery.

Chưa có ảnh logic analyzer cho giao dịch write/read bình thường hoặc recovery; chưa đo số lần ngắt EV, độ trễ START–EV5 và số xung SCL thực tế.

| Lần thử | Kết quả (OK / timeout / recovered) | Số xung SCL cần toggle |
|---|---|---|
| Chưa có chuỗi thử chèn lỗi có kiểm soát | Chưa đánh giá | Chưa đo |

Số lần phục hồi liên tiếp được xác nhận: chưa có lượt nào; yêu cầu là 11 lần. Đây là phần chưa kiểm chứng, không phải kết quả của 11 lần thử thất bại. Kiểm tra tools/check_api.sh trên week2/ex1 và week2/ex2: PASS, phạm vi 9 file.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

Thời gian quan sát lớn hơn timeout cấu hình 28 ms khoảng 53–125 µs. Phép đo bao quanh mpu_prepare_locked(), gồm các bước giao tiếp và xử lý lỗi liên quan; đây không phải phép đo riêng thời gian chờ notification hay thời gian bus recovery thành công. Chưa đủ dữ liệu để phân bổ phần chênh lệch cho ISR, polling GPIO hoặc cleanup.

Chưa có số đo để so với dự đoán tối đa 9 xung SCL và 1–5 ms từ phát hiện bus-stuck đến giao dịch tiếp theo thành công. Mã BUS có thể xuất phát từ nhiều nhánh lỗi; log hiện tại chưa xác định được nguyên nhân. Cần kiểm tra nguồn, mức SCL/SDA, pull-up và đối chiếu SR1/SR2 cùng ảnh logic analyzer trước khi kết luận.

## Trả lời câu hỏi trade-off

Khi ngoại vi I2C bị kẹt, tiếp tục yêu cầu START không bảo đảm tạo được chuỗi clock cần thiết. Driver tắt I2C, chuyển PB6/PB7 sang GPIO open-drain để điều khiển SCL thủ công, đồng thời đọc IDR để xác nhận chân thực sự lên HIGH. Sau khi SDA được nhả, driver tạo START/STOP rồi trả chân về AF và cấu hình lại I2C.

STOP là chuyển SDA từ LOW lên HIGH khi SCL HIGH, giúp kết thúc trạng thái giao tiếp của slave. Nếu bỏ STOP, slave có thể còn chờ phần còn lại của giao dịch, khiến lần giao tiếp tiếp theo lỗi hoặc BUSY chưa được giải phóng. Đây là lập luận thiết kế; chưa thực hiện phép thử bỏ STOP để chứng minh trên board.

W2.1 hiện chưa đạt tiêu chí Output. Phần còn lại là xử lý lỗi INIT, ghi nhận giao dịch bình thường, chụp dạng sóng recovery và xác nhận đủ 11 lần liên tiếp không reset MCU. RECOVERED chỉ báo bus đã được phục hồi; cần một lần đọc mẫu OK tiếp theo để xác nhận giao tiếp hoạt động lại.
