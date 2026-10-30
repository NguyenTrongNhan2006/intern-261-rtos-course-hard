# W2.4 – PREDICTION

MSSV **2452882**: barrier 3 task, mask `0x07`, timeout 101 ms; STM32F103 chạy 72 MHz, build `-O2`.

> Rà cách trình bày ngày 30/10/2026. Giữ các con số định lượng đã có trong working tree khi đối chiếu lượt đo; kết quả nằm trong [REPORT.md](REPORT.md). Git history trước lần rà này chỉ có scaffold, chưa có commit riêng của bản định lượng trước đo. Không xem lần biên tập sau đo là prediction mới hoặc đổi số dự đoán cho khớp kết quả.

## STEP 1 — Barrier

Mỗi task đóng góp một bit riêng: 0x01, 0x02 hoặc 0x04, rồi chờ đủ mask 0x07 bằng xEventGroupSync. Thiết kế cho ba task đến barrier gần 1, 2 và 3 ms nên dự kiến không timeout.

Khi task cuối cùng đến, các task đang chờ được chuyển sang Ready, nhưng Cortex-M3 chỉ thực thi từng task. Dự đoán từ lúc task cuối gọi xEventGroupSync đến khi cả ba task đều trở về khỏi hàm khoảng **500–3000 cycle**, tương đương khoảng 7–42 µs. Không xuất UART trong khoảng đo này. Đây là thời gian tới lần return cuối, không phải thời gian các task thực thi đồng thời.

## STEP 2 — Bug mất sự kiện

Sender có priority cao hơn receiver, gửi hai notification liên tiếp rồi chờ đến cặp tiếp theo, chu kỳ dự kiến 10 ms. Thử nghiệm gồm 500 cặp, tổng 1000 lần gửi.

Với eSetValueWithOverwrite, receiver chưa kịp chạy giữa hai lần gửi nên giá trị thứ nhất bị ghi đè. Dự đoán receiver nhận **500 notification, mất 500 sự kiện**. Payload nhận được là các số thứ tự 2, 4, ..., 1000; không được cộng số thứ tự payload để tính số sự kiện nhận.

Log SEND/RECV phải chứa timestamp và giá trị để đối chiếu từng cặp. Nếu trace bị tràn bộ đệm, lượt thu đó không đủ để kết luận mất notification.

## STEP 3 — Sau khi sửa

Chọn eIncrement ở phía gửi. Receiver dùng xTaskNotifyWait với clear-on-entry bằng 0, clear-on-exit bằng UINT32_MAX, rồi cộng giá trị nhận được vào tổng sự kiện.

Dự đoán mỗi cặp gửi tạo giá trị đếm **2**; khoảng **500 lần nhận** sẽ cộng thành **1000 sự kiện**, bằng tổng số gửi. Điều kiện là không có nơi khác xóa notification và số sự kiện chưa xử lý không làm tràn bộ đếm 32 bit.

Cơ chế này bảo toàn số lần xảy ra sự kiện, không lưu payload riêng của từng lần gửi. Không thể khẳng định đúng với mọi tốc độ gửi nếu bỏ qua giới hạn bộ đếm và khả năng xử lý.

## STEP 4 — RAM và cycle

Các kích thước dùng trong bản dự đoán là `sizeof(StaticEventGroup_t)=28 B`, `sizeof(StaticSemaphore_t)=80 B`, `sizeof(StaticTask_t)=88 B` khi bật notification. Event Group vẫn cần cho barrier nên không bị loại bỏ sau khi đổi cơ chế báo hiệu.

Nếu cả hai phiên bản đều bật notification, bỏ một binary semaphore giảm 80 byte bộ nhớ đối tượng. Nếu baseline tắt notification, TCB tăng từ 80 lên 88 byte; tính ba task ứng dụng và Idle, phần tăng là 32 byte, nên mức giảm ròng dự kiến là 48 byte. Các số này chưa gồm stack, tracer và những biến khác.

Bản benchmark giữ cả semaphore và notification trong cùng firmware vẫn chiếm bộ nhớ semaphore; không được coi đó là RAM đã tiết kiệm trên linker map.

Đo cùng đường báo hiệu–nhận không blocking, cùng mức tối ưu -O2, loại overhead đọc DWT và không ghi UART trong cửa sổ đo. Giữ dự đoán **binary semaphore 250–500 cycle/cặp give/take**, **notification 120–300 cycle/cặp notify/wait**. Đây là ước lượng fast path, không bao gồm context switch khi đánh thức Receiver đang Blocked; chưa phải một bảo đảm notification nhanh hơn.

## STEP 5 — Cách đối chiếu

Đếm tất cả marker S/R và phân biệt payload sequence với count. Với bản fixed, so số S với tổng count ở R, không so số S với số lời gọi receive. Kiểm tra TRACE_DROPPED và record count trước khi kết luận mất sự kiện. Tách trace gửi/nhận với Gantt chuyển task vì hai buffer có thể dừng ở những thời điểm khác nhau.

So đúng metric barrier, RAM đối tượng và cặp API của benchmark. Nếu số đo ngoài khoảng dự đoán thì ghi kết quả và giới hạn giải thích trong REPORT, không thay các khoảng ở STEP 4 bằng số thực nghiệm. Không suy ra hiệu năng ở mọi tốc độ hoặc khả năng lưu payload từ một bộ đếm sự kiện.
