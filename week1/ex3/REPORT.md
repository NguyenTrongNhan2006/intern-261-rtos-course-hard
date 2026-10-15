# W1.3 REPORT: RM, RTA và đo trên STM32

MSSV: 2452882. Báo cáo phân tích hai UART capture được người học cung cấp
trong phiên này: `week1/ex3/trace_O0.csv` và `trace_O2.csv`.
Quy đổi DWT dùng HCLK danh định 72 MHz, tương ứng 72 cycles/us.

## 1. Prediction và lịch sử

Bản prediction đã được soạn tại `deliverables/W1E3_code/week1/ex3/PREDICTION.md`
trong bước chuẩn bị. Tuy nhiên khi kiểm tra sau capture O0, PREDICTION.md trong
repo đích vẫn là template trống, HEAD là 6271a5d của W1E2. Chưa có bằng chứng
commit prediction W1E3 trước khi chạy. Không ghi nhận một commit sau phép đo
thành commit prediction trước phép đo. Cần bổ sung nội dung vào repo và giữ
đúng trình tự thời gian này trong báo cáo.

Firmware commit và bản lưu ELF/map đúng hai lần đo: [CẦN BỔ SUNG].

## 2. Mô hình lý thuyết

Bốn task độc lập, periodic, preemptive fixed priority RM, D = T,
không blocking tài nguyên. Mô hình RTA cơ sở bỏ qua scheduler/tracer/ISR.

| Task | T = D (ms) | C (us) | Priority | Ui | RTA (ms) |
| :--- | ---: | ---: | ---: | ---: | ---: |
| tau0 | 60 | 9909 | 4 | 0.16515 | 9.909 |
| tau1 | 100 | 10062 | 3 | 0.10062 | 19.971 |
| tau2 | 200 | 31486 | 2 | 0.15743 | 51.457 |
| tau3 | 400 | 74591 | 1 | 0.1864775 | 155.928 |

U = 0.6096775 < U_LL = 4*(2^(1/4)-1) = 0.756828460.
Tất cả RTA <= D. Task set gốc schedulable trong mô hình đã nêu,
không cần đổi T/C/priority. Chuỗi RTA theo ms:

tau0: 9.909 -> 9.909.

tau1: 10.062 -> 19.971 -> 19.971.

tau2: 31.486 -> 51.457 -> 51.457.

tau3: 74.591 -> 135.957 -> 155.928 -> 155.928.

## 3. Phương pháp và tính đầy đủ capture

H = LCM(60,100,200,400) = 1200 ms. Release trong 5H = 6000 ms.
Epoch chung được lấy trong tick hook tại tick 20. Release danh định đặt
trên lưới epoch + k*T. Response = finish - nominal release.

Mỗi capture có đúng một marker DONE, 100/60/30/15 job của tau0/1/2/3,
tổng 205. Parser xác nhận chung epoch, không thiếu/trùng release và
selected-time budget từng job ít nhất bằng C quy đổi sang cycles.
Cả O0/O2 có zero deadline miss trong tập job đã quan sát.

JOB recording dùng storage riêng và tiếp tục sau khi scheduler ring freeze.
Mỗi scheduler capture có 254 event, overflow=0 và 126 cặp OUT->next IN.
Gantt chỉ thể hiện cửa sổ ngắn của scheduler ring, không phải toàn bộ 5H.

Ảnh terminal người học gửi xác nhận flash O0 báo Verified OK rồi capture
hoàn tất. Bằng chứng flash O2 riêng: [CẦN BỔ SUNG].

## 4. WCRT quan sát

| Task | Số job mỗi cấu hình | RTA (ms) | WCRT O0 (ms) | Delta O0 (us) | WCRT O2 (ms) | Delta O2 (us) |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| tau0 | 100 | 9.909 | 9.934583 | 25.583 | 9.924681 | 15.681 |
| tau1 | 60 | 19.971 | 20.033486 | 62.486 | 20.007403 | 36.403 |
| tau2 | 30 | 51.457 | 51.570069 | 113.069 | 51.521917 | 64.917 |
| tau3 | 15 | 155.928 | 156.243194 | 315.194 | 156.102944 | 174.944 |

Delta = observed WCRT - ideal RTA. O0 lớn hơn O2 lần lượt
9.903 / 26.083 / 48.153 / 140.250 us.
Đây là maximum hữu hạn trong capture, không phải chứng minh WCRT phần cứng
tuyệt đối. Với cả hai cấu hình, job gây maximum của tau3 có 3 resumes sau
khi bắt đầu phần việc, phù hợp 3 preemption trong lịch critical-instant.

## 5. Khoảng OUT->next IN của firmware W1E3

| Optimization | Mẫu | Min (cycles) | Max (cycles) | Mean (cycles) | Jitter (cycles) |
| :--- | ---: | ---: | ---: | ---: | ---: |
| O0 | 126 | 460 | 627 | 529.642857 | 167 |
| O2 | 126 | 224 | 281 | 248.507937 | 57 |

| Optimization | Min (us) | Max (us) | Mean (us) | Jitter (us) |
| :--- | ---: | ---: | ---: | ---: |
| O0 | 6.388889 | 8.708333 | 7.356151 | 2.319444 |
| O2 | 3.111111 | 3.902778 | 3.451499 | 0.791667 |

Mean OUT->IN giảm khoảng 53.08% từ O0 sang O2. Đây là khoảng giữa hai
timestamp của hook, không bao phủ toàn bộ exception entry/exit và PendSV
save/restore. Dùng số đo W1E3 này khi đối chiếu firmware W1E3; baseline
W1E2 d673d5f có đường thực thi và số liệu khác.

## 6. Đối chiếu định lượng trong cửa sổ job gây maximum

Cửa sổ scheduler ring hiện có chứa job gây maximum của cả bốn task ở cả
hai cấu hình. Bảng dưới chỉ cộng cặp có timestamp OUT >= release và
timestamp IN kế tiếp <= finish. Cặp cắt ngang biên cửa sổ không được cộng.
Một cặp hook có thể cùng tag ở hai đầu; đếm cặp hook không đồng nhất
với đếm lần đổi sang task khác hoặc số preemption của riêng task đó.

| Task | Cặp O0 | Tổng OUT->IN O0 (us) | Cặp O2 | Tổng OUT->IN O2 (us) |
| :--- | ---: | ---: | ---: | ---: |
| tau0 | 1 | 6.388889 | 1 | 3.111111 |
| tau1 | 2 | 13.722222 | 2 | 6.555556 |
| tau2 | 3 | 22.083333 | 4 | 13.500000 |
| tau3 | 10 | 74.527778 | 11 | 38.111111 |

Ví dụ tau3 O0: Delta = 315.194444 us; các khoảng OUT->IN trong response
window cộng lại 74.527778 us. Chênh số học còn lại là 240.666667 us.
O2: Delta = 174.944444 us, tổng OUT->IN = 38.111111 us, chênh số học
còn lại 136.833333 us. Do đó riêng khoảng OUT->IN chưa giải thích đầy đủ Delta.

Đây là phép đối chiếu thời gian, chưa phải phân rã causal độc lập.
Selected-time accounting có thể charge một phần exception vào C, nên
không được mặc định mọi overhead là các thành phần rời nhau để cộng trừ.
Phần chưa định lượng cần đo thêm gồm tick ISR, các đoạn PendSV/hook ngoài
hai timestamp, code đầu/cuối job, release-grid offset và hiệu chỉnh C.

Maximum overshoot của selected-time budget theo us:

| Task | O0 | O2 |
| :--- | ---: | ---: |
| tau0 | 2.778 | 1.764 |
| tau1 | 1.097 | 3.597 |
| tau2 | 2.861 | 1.736 |
| tau3 | 2.667 | 1.764 |

Các maximum overshoot không nhất thiết xảy ra trong chính job gây WCRT.
Selected-time loại thời gian task khác chạy, vẫn gồm một phần ISR và
exception; các số này không tự chứng minh exclusive CPU cost đúng C.
Hiệu chỉnh C và đo residual riêng: [CẦN BỔ SUNG].

### 6.1. Tổng budget của các job trong response window

Kiểm tra bổ sung trên JOB log: không có job ưu tiên cao carry-in qua
release của job gây maximum đang xét. Số job góp phần trong cửa sổ của
tau0/tau1/tau2 lần lượt là (1,0,0,0), (1,1,0,0), (1,1,1,0).
Với tau3 là (3,2,1,1): ba job tau0, hai job tau1, một job tau2 và chính
job tau3. Tổng C danh định tương ứng đúng các giá trị RTA.

Gọi S là tổng selected_cycles của các job này, G là tổng khoảng OUT->IN
nằm trọn trong response window và R là response quan sát, tất cả cùng
đơn vị cycles. Đặt E = S - tổng C quy đổi cycles và B = R - S - G.
Khi đó Delta = E + G + B. Đây là đẳng thức từ các timestamp/counter của
log; B là phần dư quan sát, chưa phải phép đo riêng một loại ISR hay hook.

| Cấu hình | Task | Delta (us) | Tổng vượt budget E (us) | OUT->IN G (us) | Phần dư B (us) |
| :--- | :--- | ---: | ---: | ---: | ---: |
| O0 | tau0 | 25.583333 | 2.361111 | 6.388889 | 16.833333 |
| O0 | tau1 | 62.486111 | 3.305556 | 13.722222 | 45.458333 |
| O0 | tau2 | 113.069444 | 5.611111 | 22.083333 | 85.375000 |
| O0 | tau3 | 315.194444 | 12.958333 | 74.527778 | 227.708333 |
| O2 | tau0 | 15.680556 | 1.763889 | 3.111111 | 10.805556 |
| O2 | tau1 | 36.402778 | 1.777778 | 6.555556 | 28.069444 |
| O2 | tau2 | 64.916667 | 3.722222 | 13.500000 | 47.694444 |
| O2 | tau3 | 174.944444 | 5.833333 | 38.111111 | 131.000000 |

Ví dụ tau3 O0: 315.194444 = 12.958333 + 74.527778 + 227.708333 us.
Selected-time đã bao gồm một phần exception/ISR, vì vậy không gán toàn bộ
B cho tick ISR hoặc PendSV. Cần instrumentation bổ sung mới tách riêng
các nguồn vật lý đó. Bảng trên cũng không xác nhận exclusive CPU cost C.

## 7. Logic Analyzer và bằng chứng còn thiếu

Mapping của W1E3: PA4 = Idle, PA5 = tau0, PA6 = tau1, PA7 = tau2,
PA8 = tau3. UART TX = PA9, 115200 baud, 8N1.

Đã đọc capture_O0.sr bằng trace_correlate.py của repo, với mapping channels
0,1,2,3,4. Session chứa 10 triệu samples ở 1 MHz, tức 10 giây.
Khớp đầy đủ 254 event GPIO với 254 event UART theo đúng thứ tự tag và
IN/OUT. Mốc cạnh đầu trong LA tại 1127819 us tính từ đầu session.

Chỉ căn origin tại cạnh đầu, không fit clock scale: max absolute mismatch
= 7.819444 us. Mean signed mismatch = -4.295057 us, SD = 1.955292 us.
Giá trị 7.819444 us dưới ngưỡng tham chiếu 16 us dùng ở W1E2; ngưỡng này
không phải một yêu cầu W1E3 riêng được sinh bởi generator.

Nếu bỏ event đầu thread-mode và fit offset hằng cho 253 event còn lại,
max absolute residual = 3.645367 us. Đây là thống kê bổ sung, không thay
thế 7.819444 us khi căn cạnh đầu, không chứng minh độ chính xác vật lý
tuyệt đối của LA hoặc oscillator. Sampling 1 MHz cho bước thời gian 1 us.

UART và LA được thu trong các lượt reset riêng theo trình tự thực hiện;
kết quả xác nhận sự khớp trình tự/timing giữa hai capture, không tuyên bố
chúng là cùng một lần chạy. Đối chiếu này cũng chưa hiệu chuẩn release
epoch trong tick hook hoặc exclusive CPU cost.

Đã kiểm tra lần capture_O2.sr đầu tiên: 10 triệu samples, 1 MHz. Checker
không tìm thấy chuỗi đầy đủ 254 event. Kiểm tra chẩn đoán tìm thấy 8 event
UART đầu khớp thứ tự, thiếu event thứ 9 (tag 0 IN, tức cạnh lên Idle trên
D0), rồi 245 event còn lại khớp liên tiếp. Căn cạnh đầu của riêng suffix
245 event cho max absolute mismatch 7.027778 us. Đây là chẩn đoán một phần,
không phải kết quả PASS của đối chiếu đầy đủ; không bỏ/sửa event UART để
ép khớp. Nguyên nhân cạnh Idle đầu không quan sát được chưa được xác định.

Lần O2 đầu đã được giữ riêng ở capture_O2_first.sr. Sau khi thu lại và lưu
capture_O2.sr mới, checker khớp đầy đủ 254 event theo thứ tự tag và IN/OUT,
với mapping D0..D4 tương ứng tag 0..4. Session mới có 10 triệu samples,
sample rate 1 MHz, thời gian 10 giây.

Max absolute mismatch khi căn cạnh đầu O2 = 8.972222 us; mean signed
mismatch = -4.535761 us, SD = 2.379146 us. Nếu loại event thread đầu và
fit offset hằng cho các event PendSV, max absolute residual = 4.418533 us.
Không thay 8.972222 us bằng số residual này khi báo cáo sai lệch căn cạnh đầu.

| Cấu hình | Event khớp | Sample rate | Max mismatch căn cạnh đầu (us) |
| :--- | ---: | ---: | ---: |
| O0 | 254 | 1 MHz | 7.819444 |
| O2, lần thu lại | 254 | 1 MHz | 8.972222 |

Cả hai dưới ngưỡng tham chiếu 16 us của W1E2. Đây không phải ngưỡng
riêng của bài W1E3. UART và LA vẫn là các lượt reset riêng; sự khớp
capture không hiệu chuẩn clock hoặc chứng minh cùng một lần thực thi.
Không dùng ảnh LA W1E2 làm bằng chứng cho firmware W1E3.
Kết quả có cấu trúc: deliverables/W1E3_correlation_O0.json và
deliverables/W1E3_correlation_O2.json.

API checker sau tích hợp: PASS (16 files), phạm vi platform, week1/ex3,
week1/ex2/tracer.c và CMakeLists.txt gốc. Dùng chính checker của repo,
chuẩn hóa newline ở bản script chạy tạm và gán Python 3 runtime.
Đây là policy scan, không thay thế bằng chứng build/flash/phần cứng.
Mục AI_LOG W1E3 đã được chuẩn bị trong deliverables/W1E3_code/AI_LOG_ENTRY.md,
chưa được thêm vào AI_LOG.md của repo. Giữ nguyên hai CSV, ELF và map đã
dùng cho từng phép đo.

## 8. Trade-off

LL là điều kiện đủ và kiểm tra nhanh nhưng có thể bảo thủ; U vượt LL
chưa chứng minh không schedulable. RTA xét interference theo từng priority
và cho kết luận chính xác trong mô hình task phù hợp. Với bộ này U dưới
LL và cả bốn RTA đều dưới deadline nên hai cách cùng kết luận đạt trên giấy.
Số đo không miss trong 205 job hỗ trợ kết luận ở cấu hình và khoảng đo này,
không thay thế việc mô hình hóa overhead/jitter/blocking của hệ thống thực.
