# W2.1 – PREDICTION

MSSV **2452882**: I2C1 100 kHz, timeout 28 ms, yêu cầu 11 recovery liên tiếp.

> Rà cách trình bày ngày 30/10/2026. Giữ các con số dự đoán trước đo; không thay bằng số đo. Bản dự đoán đã có trong commit `85c8cde` ngày 20/10/2026. Kết quả và giới hạn kiểm chứng nằm trong [REPORT.md](REPORT.md).

## STEP 1 — Giao dịch và ngắt

| Đại lượng | Dự đoán trước đo | Điều kiện |
|---|---|---|
| Số lần vào ISR EV khi ghi N byte | Khoảng N+3: SB, ADDR, N lượt TXE, BTF kết thúc | Các sự kiện được xử lý riêng; cờ cùng xuất hiện có thể làm số lần khác |
| START → xử lý EV5 | Vài µs | Bus rảnh, không bị ngắt ưu tiên cao trì hoãn |

Đọc 1/2/N byte cần xử lý ACK/POS/STOP theo độ dài. Số ngắt của nhánh RX phụ thuộc FSM; không suy ra bằng công thức TX N+3.

## STEP 2 — Timeout

Task chờ notification tối đa theo timeout cấu hình **28 ms**. Thời gian quan sát từ bên ngoài còn có tick, lịch task và thao tác recovery. Chưa cấu hình watchdog cho bài; nếu bổ sung watchdog thì phải tính thời gian còn lại của watchdog và khả năng task bị trì hoãn.

## STEP 3 — Bus recovery

| Đại lượng | Dự đoán trước đo |
|---|---|
| Số xung SCL khi slave giữ SDA LOW | Tối đa 9; dừng sớm nếu SDA đã HIGH |
| Số lần recovery liên tiếp | Đạt ít nhất 11 nếu tác động lỗi được gỡ và slave hoạt động lại |
| Phát hiện bus-stuck → transaction kế tiếp thành công | Khoảng 1–5 ms nếu lỗi đã gỡ, giao dịch ngắn và task được chạy ngay |

Recovery không được reset MCU. Một lượt chỉ tính thành công sau khi routine recovery kết thúc và một transaction sau đó thực sự OK. Nếu bên ngoài tiếp tục giữ SDA LOW thì không bảo đảm recovery thành công.

## STEP 4 — Bằng chứng cần thu

Thu UART có streak và kết quả transaction; thu SCL/SDA cùng marker EV/ER trên logic analyzer. Phân biệt thời điểm gây lỗi, phát hiện lỗi, bắt đầu recovery và giao dịch kế tiếp OK. Nếu chỉ có số `US` của lời gọi client thì chưa đo được tất cả khoảng thời gian trên.

Xung SDA ngắn kiểm tra đường xử lý lỗi nhưng có thể nhả trước khi recovery bắt đầu. Muốn kiểm chứng vòng phát clock phải có SDA LOW tại lúc bắt đầu recovery và quan sát xung SCL; phần này là tiêu chí đo được làm rõ khi rà hồ sơ, không phải số liệu dự đoán mới.
