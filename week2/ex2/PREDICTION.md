# W2.2 – PREDICTION

MSSV **2452882**. Tham số: 512 slot, 1,000,000 phần tử stress, 7,048 mẫu benchmark mỗi cơ chế; Cortex-M3 72 MHz, `-O2`.

> Rà cách trình bày ngày 30/10/2026. Giữ các số dự đoán trước đo trong commit `b236159` ngày 21/10/2026. Số đo đặt trong [REPORT.md](REPORT.md), không thay vào các ô dự đoán dưới đây.

## STEP 1 — Dự đoán đúng đắn

Dự đoán nhận đủ seq **0…999999**, không mất, lặp hoặc đảo thứ tự; overrun bằng 0 nếu consumer theo kịp producer. Ring dùng đủ 512 slot. Nếu push thất bại do đầy phải ghi overrun, không bỏ qua rồi kết luận PASS.

Một producer ghi head, một consumer ghi tail. Load/store uint32_t căn chỉnh 4 byte là atomic trên Cortex-M3. DMB giữ thứ tự quan sát chỉ số, truy cập payload và công bố chỉ số; volatile không thay barrier. Dự đoán an toàn chỉ áp dụng cho mô hình SPSC này.

## STEP 2 — Dự đoán cycle

| Cơ chế | Push/send | Pop/receive | Tổng cycle/cặp |
|---|---:|---:|---:|
| Ring tự viết | khoảng 30 | khoảng 30 | khoảng 60 |
| FreeRTOS Queue | khoảng 100 | khoảng 140 | khoảng 240 |

Dự đoán Queue/Ring khoảng **4 lần**. Queue có critical section BASEPRI, copy tổng quát, quản lý trạng thái và kiểm tra waiting list. Ring cũng copy phần tử nhưng kích thước cố định 4 byte, chỉ số vòng dùng mask.

Trong 240 cycle dự đoán cho Queue, phân bổ giả định: 60 cho critical section, 60 cho copy/cập nhật con trỏ, 20 cho kiểm tra waiting list rỗng và 100 cho phần còn lại. Đây là giả thuyết để đối chiếu; chưa đo tách và không xem là chi phí cố định từ tài liệu.

## STEP 3 — Điều kiện phép đo

Đo DWT quanh API: push/send trong ISR, pop/receive trong task. Hai cơ chế cùng kiểu phần tử và dung lượng; hiệu chỉnh chi phí đọc DWT. Không tính log UART, kiểm tra seq, thời gian chờ, vào/ra ISR hoặc yield sau send vào cycle API.

Benchmark từng mẫu: producer đợi consumer lấy mẫu trước; Queue receive không blocking và waiting list rỗng. Vì vậy dự đoán trên không bao gồm đánh thức task. Stress 1 triệu phần tử chạy riêng để kiểm tra đúng đắn, không thay cho benchmark cycle.

## STEP 4 — Tiêu chí đối chiếu

Lưu produced, received, overrun và lỗi seq đầu tiên nếu có. Điền push/pop trung bình, thời gian quy đổi ở 72 MHz và tỷ lệ Queue/Ring; nêu rõ nếu số trung bình bị chia nguyên hoặc có ngắt chen ngang. Không gán chi phí từng nhóm lệnh chỉ từ một trung bình tổng.
