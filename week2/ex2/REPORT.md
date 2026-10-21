# W2.2 – REPORT

MSSV: 2452882. STM32F103C8T6 chạy 72 MHz, build -O2. Ring và Queue cùng chứa 512 phần tử, mỗi phần tử 4 byte. TIM2 phát ngắt mỗi 100 µs; một ISR producer và một task consumer. Stress test dùng 1000000 phần tử; benchmark mỗi cơ chế dùng 7048 mẫu thành công.

## Số đo

Log UART chép từ ảnh chụp kết quả trên board:

```text
PHASE,STATUS,PRODUCED,RECEIVED,OVERRUN,MEAN_PUSH,MEAN_POP
# DWT_OVERHEAD=1
STRESS,PASS,1000000,1000000,0,NA,NA
RING,PASS,7048,7048,0,29,24
QUEUE,PASS,7048,7048,0,210,197
```

Consumer đã kiểm tra seq từ 0 đến 999999: không phát hiện mất, lặp hoặc đảo thứ tự; overrun bằng 0. Ảnh hiển thị hai lượt hoàn chỉnh có cùng kết quả. NA ở lượt stress nghĩa là không xuất cycle trung bình cho lượt này.

| Cơ chế | Cycle trung bình (push/send) | Cycle trung bình (pop/receive) | Tổng theo log (cycle/cặp) |
|---|---:|---:|---:|
| SPSC ring buffer | 29 | 24 | 53 |
| xQueueSendFromISR / xQueueReceive | 210 | 197 | 407 |

Các trung bình được in dưới dạng số nguyên làm tròn xuống. Tổng từ log tương ứng khoảng 0,736 µs/cặp cho ring và 5,653 µs/cặp cho Queue; tỷ lệ Queue/ring xấp xỉ 7,68. Kiểm tra tools/check_api.sh trên week2/ex1 và week2/ex2: PASS, phạm vi 9 file.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

PREDICTION dự kiến ring 30 + 30 = 60 cycle/cặp, Queue 100 + 140 = 240 cycle/cặp và tỷ lệ 4 lần. Kết quả ring giảm 7 cycle, khoảng 11,7%; Queue tăng 167 cycle, khoảng 69,6%. Phần tăng của Queue gồm 110 cycle ở send và 57 cycle ở receive. Tỷ lệ đo được khoảng 7,68 lần lớn hơn dự đoán ban đầu.

Ring xử lý cố định phần tử 4 byte, chỉ số vòng bằng mask 511 và các DMB. Queue có thêm critical section dùng BASEPRI, ISB/DSB của port, cập nhật trạng thái và con trỏ, memcpy theo kích thước runtime, kiểm tra waiting list và các assert. Ring cũng copy payload, nên không thể giải thích lợi thế bằng việc ring không copy. Chưa đo riêng từng nhóm lệnh; phân bổ 60/60/20/100 cycle trong PREDICTION vẫn là giả thuyết, chưa được xác nhận.

Benchmark đo quanh API bằng DWT và hiệu chỉnh phép đo rỗng 1 cycle; log UART và kiểm tra seq nằm ngoài khoảng đo. Producer chờ consumer lấy mẫu trước rồi mới tạo mẫu mới; xQueueReceive dùng thời gian chờ 0 nên waiting list rỗng, không có chi phí đánh thức task do Queue trong thiết lập này. portYIELD_FROM_ISR nằm ngoài khoảng đo API. Ngắt chen ngang vẫn có thể ảnh hưởng trung bình; kết quả không đại diện cho trường hợp consumer blocking hoặc nhiều client.

## Trả lời câu hỏi trade-off

Chỉ producer ghi head và chỉ consumer ghi tail; load/store uint32_t căn chỉnh 4 byte là atomic trên Cortex-M3. Producer ghi payload trước khi công bố head; consumer quan sát head rồi đọc payload, hoàn tất đọc trước khi công bố tail. DMB giữ thứ tự bàn giao, còn volatile không tự thay thế barrier. Nếu ISR chen vào lúc task đang đọc, slot vẫn chưa được trả qua tail nên producer không được ghi đè. Mỗi push/pop trả về sau số bước hữu hạn, không dùng lock hay biến count bị hai bên cùng read-modify-write.

Nếu thêm producer thứ hai, producer A có thể đọc head = 0 rồi bị producer B ưu tiên cao hơn chen vào. B ghi storage[0] và công bố head = 1; A tiếp tục ghi đè storage[0] rồi cũng công bố head = 1, làm mất phần tử của B. DMB không giải quyết tranh chấp quyền ghi này. Payload nhiều field vẫn có thể an toàn nếu ghi hoàn chỉnh trước publish và giữ đúng quyền sở hữu slot; riêng việc lớn hơn một word chưa đủ để kết luận SPSC sai.

Stress test xác nhận không có lỗi trong các lượt đã chạy; lập luận an toàn dựa trên mô hình SPSC, quyền sở hữu slot và memory ordering. Kết quả áp dụng cho cấu hình Cortex-M3 đơn nhân hiện tại, không tự suy rộng sang nhiều producer/consumer hoặc SMP.
