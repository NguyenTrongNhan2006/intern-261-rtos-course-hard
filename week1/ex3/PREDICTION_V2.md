# W1E3 V2: dự đoán trước phép đo mới

MSSV 2452882. Bản bổ sung này được soạn sau khi đã xem kết quả V1,
sau khi build/host-test source V2, nhưng trước khi flash/đo V2 trên STM32.
Không thay thế lịch sử prediction V1 và không gọi đây là dự đoán mù.

## 1. Thay đổi và lý do

V1 dùng selected-time làm điều kiện kết thúc busy-wait, còn tính một phần
ISR/exception vào C. V2 chạy đủ số vòng SUBS/BNE cố định; ngắt không làm
giảm số vòng còn lại. Hiệu chỉnh bằng DWT trước scheduler, IRQ tắt trong
lúc hiệu chỉnh. Cùng một hàm đo và cùng vòng assembly được dùng lúc chạy.

SysTick/PendSV được bọc bằng wrapper riêng của W1E3; source kernel không
bị sửa. PendSV wrapper giữ EXC_RETURN trên MSP, không phục hồi r4-r11 của
task cũ sau khi native handler đã chuyển context. Hai exception cùng mức
priority thấp nhất; profiler assert không có nesting giữa hai handler.

## 2. Dự đoán calibration

C mục tiêu giữ nguyên 9909 / 10062 / 31486 / 74591 us.
Calibrate số vòng bằng binary search; kiểm tra 8 mẫu không ngắt.
Dự kiến min >= C yêu cầu và max <= C + 1 us. Firmware assert điều kiện
này và log target/iterations/min/max; chưa có số đo V2 để điền trước.
Đây là hiệu chỉnh thực nghiệm ở clock danh định, không proof WCET tuyệt đối.

T, D và priority RM giữ nguyên. U danh định = 0.6096775; LL = 0.756828460.
RTA danh định vẫn 9.909 / 19.971 / 51.457 / 155.928 ms.
Nếu dùng C + 1 us làm cost, RTA tương ứng 9.910 / 19.973 / 51.460 /
155.935 ms; các phép ceil chưa đổi ở mức tăng này. Dự kiến vẫn không miss
deadline với bộ tham số này sau khi thêm overhead thực tế.

## 3. Phép đo và phân tích dự kiến

Release được timestamp ngay trong tick hook sau khi kernel cập nhật tick,
cùng một timestamp cho các task release cùng tick. Đây là mốc release
được đo, khác lưới cycle danh định của V1. Analyzer phải báo sai lệch so
với epoch + k*T và không âm thầm coi hai định nghĩa là giống nhau.

Mỗi log có 205 JOB2 trong 5H = 6000 ms. Mỗi job lưu response window,
work wall time, cycle và count SysTick/PendSV nằm trong response window.
SysTick tại release đã chạy một phần: chỉ phần còn lại sau release được
tính vào response. UART dump sau khi bốn task đã hoàn tất.

Dự kiến V2 WCRT khác V1 vì workload không còn credit thời gian ISR và vì
profiler có chi phí riêng. Không gán các WCRT V1 cho firmware V2.

Delta = observed R - ideal RTA. Đối chiếu tổng C đã hiệu chỉnh của các
job góp phần, SysTick bracket, PendSV bracket và residual ngoài bracket.
PendSV bracket đã gồm tracer/scheduler/software save-restore; không cộng
OUT->IN thêm lần nữa. Hardware exception entry/exit, tail chaining và
một phần mép wrapper nằm ngoài bracket, phải nêu trong giới hạn đo.
Empty enter/exit probe được log riêng để thấy observer cost, không tự
trừ như một hiệu chỉnh chính xác cho mọi handler.

## 4. Bằng chứng và tên file

Giữ nguyên trace_O0.csv, trace_O2.csv và capture_O0/O2.sr của V1.
V2 dùng build/w1e3-v2-O0, build/w1e3-v2-O2 và trace_v2_O0/O2.csv.
Capture LA mới nếu lấy thì dùng capture_v2_O0/O2.sr.
Prediction V2 cần commit trước khi thực hiện các phép đo mới.
