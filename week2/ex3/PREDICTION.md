# W2.3 – PREDICTION (Dự đoán định lượng trước khi chạy thực nghiệm)

Theo params.h của MSSV 2452882, priority của tau_L, tau_M1, tau_M2, tau_H lần lượt là 2, 3, 5, 7; workload vùng găng A/B là 19/15 ms. Dự đoán bỏ qua overhead và sai số tick, tính workload theo thời gian CPU thực thi, không tính thời gian bị preempt. Các mốc kích hoạt dùng delay; tau_M1 không Ready trong khoảng đang phân tích.

## Dự đoán Hiện tượng 1: Đảo ngược độ ưu tiên không giới hạn (Unbounded Priority Inversion)

Dùng xSemaphoreCreateBinaryStatic cho A, không có priority inheritance. tau_L lấy A tại t = 0 ms; tau_H yêu cầu A tại t = 2 ms và bị Blocked. tau_M2 chạy 30 ms từ t = 4 đến t = 34 ms, khiến tau_L bị preempt. tau_L đã thực thi 4 ms nên còn 15 ms, hoàn thành và nhả A tại t = 49 ms. Dự đoán tau_H chờ 47 ms. Nếu task trung gian liên tục chiếm CPU, thời gian chờ có thể tăng không giới hạn.

## Dự đoán Hiện tượng 2: Hiện tượng chặn nối tiếp (Chained Blocking với Mutex PIP)

Dùng xSemaphoreCreateMutexStatic cho A/B. tau_L lấy A tại t = 0 ms, tau_M2 lấy B tại t = 1 ms. Khi tau_H yêu cầu A tại t = 2 ms, tau_L kế thừa priority 7 và chạy nốt 18 ms, nhả A gần t = 20 ms. tau_H lấy rồi nhả A, tiếp tục yêu cầu B và bị chặn lần hai. tau_M2 kế thừa priority 7, chạy nốt 14 ms và nhả B gần t = 34 ms. Dự đoán tau_H bị chặn hai lần, tổng 32 ms; tổng workload hai vùng găng là 34 ms.

## Dự đoán Hiện tượng 3: Hiện tượng bế tắc kinh điển (Classical Deadlock)

tau_L lấy A tại t = 0 ms; tau_H lấy B tại t = 2 ms rồi yêu cầu A tại t = 3 ms. tau_H bị Blocked, tau_L kế thừa priority 7 nhưng yêu cầu B tại t = 4 ms và cũng bị Blocked. Nếu cả hai chờ bằng portMAX_DELAY, circular wait hình thành và không tự kết thúc. Priority inheritance không giải phóng khóa; các task khác hoặc Idle vẫn có thể chạy.

## Dự đoán Giải pháp 4: Trần độ ưu tiên tức thời (Immediate Priority Ceiling Protocol - IPCP)

Đặt ceiling của A/B bằng 7 và configUSE_TIME_SLICING = 0. Mọi task dùng wrapper, không delay hoặc yield khi giữ khóa và unlock theo LIFO. Trong hai kịch bản đầu, tau_L được nâng lên 7 khi lấy A, nên tau_M2 không thể chen vào giữ B. tau_L nhả A gần t = 19 ms; tau_H được kích hoạt tại t = 2 ms phải chờ khoảng 17 ms trước khi chạy.

Khoảng chờ này xảy ra ở trạng thái Ready, nên thời gian đo riêng trong xSemaphoreTake có thể gần 0. Với các vùng găng không lồng nhau, blocking do task thấp hơn được giới hạn vào một vùng găng, tối đa khoảng 19 ms cộng overhead. Nếu lồng đủ workload B bên trong A, phải tính cả vùng găng ngoài, có thể khoảng 34 ms.

Trong kịch bản khóa chéo, ceiling ngăn task còn lại chen vào giữ khóa thứ hai, nên dự đoán không hình thành deadlock dưới các điều kiện trên. Có thể kiểm chứng thêm bằng cách thống nhất thứ tự lấy A→B, nhả B→A để loại circular wait.

## Bảng tổng hợp đối chiếu định lượng các kịch bản

| Hiện tượng / Cơ chế | Số lần chờ | Mốc dự kiến (ms) | Thời gian chờ của tau_H |
| --- | --- | --- | --- |
| Binary Semaphore | 1 lần Blocked | 2–49 | Khoảng 47 ms; tăng theo workload trung gian |
| Mutex PIP – chained blocking | 2 lần Blocked | 2–20 và 20–34 | Tổng khoảng 32 ms |
| Mutex PIP – deadlock | Không thoát chờ | H chờ từ 3; vòng chờ hoàn tất tại 4 | Không hữu hạn nếu không can thiệp |
| IPCP – vùng găng không lồng | 1 khoảng chờ Ready | 2–19 | Khoảng 17 ms; giới hạn vùng găng lớn nhất khoảng 19 ms |

Đây là dự đoán trước thực nghiệm. Commit trước khi chạy, sau đó đối chiếu bằng Gantt từ tracer W1.2 và bảng thời gian chờ trong REPORT.md.
