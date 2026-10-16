# W1.3 REPORT: RM, RTA và đo trên STM32

MSSV: 2452882. Báo cáo phân tích hai UART capture được người học cung cấp
trong phiên này: `measurements/trace_O0.csv` và `measurements/trace_O2.csv`.
Quy đổi DWT dùng HCLK danh định 72 MHz, tương ứng 72 cycles/us.

## 1. Prediction và lịch sử

Bản prediction đã được soạn tại `deliverables/W1E3_code/week1/ex3/PREDICTION.md`
trong bước chuẩn bị. Tuy nhiên khi kiểm tra sau capture O0, PREDICTION.md trong
repo đích vẫn là template trống, HEAD là 6271a5d của W1E2. Chưa có bằng chứng
commit prediction W1E3 trước khi chạy. Không ghi nhận một commit sau phép đo
thành commit prediction trước phép đo. Cần bổ sung nội dung vào repo và giữ
đúng trình tự thời gian này trong báo cáo.

Source V1 được checkpoint tại a624a01 sau các phép đo; ELF/map gốc đã
được backup. SHA256 nằm trong measurements/BASELINE_V1_MANIFEST.json. Không gọi commit
sau phép đo là commit prediction trước phép đo.

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
V1 chưa hiệu chỉnh exclusive C hoặc tách ISR. V2 khắc phục cách tạo
workload và bổ sung profiling ở mục 9; không áp kết quả V2 ngược cho V1.

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

Đã đọc measurements/capture_O0.sr bằng trace_correlate.py của repo, với mapping channels
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

Đã kiểm tra lần measurements/capture_O2.sr đầu tiên: 10 triệu samples, 1 MHz. Checker
không tìm thấy chuỗi đầy đủ 254 event. Kiểm tra chẩn đoán tìm thấy 8 event
UART đầu khớp thứ tự, thiếu event thứ 9 (tag 0 IN, tức cạnh lên Idle trên
D0), rồi 245 event còn lại khớp liên tiếp. Căn cạnh đầu của riêng suffix
245 event cho max absolute mismatch 7.027778 us. Đây là chẩn đoán một phần,
không phải kết quả PASS của đối chiếu đầy đủ; không bỏ/sửa event UART để
ép khớp. Nguyên nhân cạnh Idle đầu không quan sát được chưa được xác định.

Lần O2 đầu được lưu trong commit a624a01 với tên capture_O2_first.sr.
Sau khi thu lại và lưu
measurements/capture_O2.sr mới, checker khớp đầy đủ 254 event theo thứ tự tag và IN/OUT,
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
AI_LOG.md đã được bổ sung phạm vi hỗ trợ W1E3 và V2. Giữ nguyên hai CSV,
ELF và map đã dùng cho từng phép đo.

## 8. Trade-off

LL là điều kiện đủ và kiểm tra nhanh nhưng có thể bảo thủ; U vượt LL
chưa chứng minh không schedulable. RTA xét interference theo từng priority
và cho kết luận chính xác trong mô hình task phù hợp. Với bộ này U dưới
LL và cả bốn RTA đều dưới deadline nên hai cách cùng kết luận đạt trên giấy.
Số đo không miss trong 205 job hỗ trợ kết luận ở cấu hình và khoảng đo này,
không thay thế việc mô hình hóa overhead/jitter/blocking của hệ thống thực.

## 9. V2: workload đã hiệu chỉnh và IRQ profiling trên STM32

Các bảng mục 1 đến 8 là baseline V1, được giữ nguyên để so sánh. Source V1
được checkpoint tại a624a01; ELF/map và dữ liệu gốc có SHA256 trong
measurements/BASELINE_V1_MANIFEST.json. Prediction V2 commit 59a16a7 trước mọi lần
nạp V2. Prediction này đã biết kết quả V1, không phải dự đoán mù.

V2 chạy đủ vòng SUBS/BNE cố định đã hiệu chỉnh DWT trước scheduler, khi
IRQ tắt. Preemption không làm giảm số vòng cần thực hiện. Cùng hàm đo
và cùng assembly workload dùng cho calibration và lúc scheduler chạy.
Mỗi C được kiểm tra 8 mẫu uncontended; đây là calibration thực nghiệm,
không là proof WCET tuyệt đối khi fetch phase thay đổi sau preemption.

SysTick/PendSV được đo bằng wrapper riêng của W1E3, không sửa kernel.
Theo chỉ dẫn indirect routing trong ARM_CM3 port, configCHECK_HANDLER_INSTALLATION
bằng 0; main thay bằng assert địa chỉ SVC/PendSV/SysTick trên vector table.
PendSV wrapper giữ EXC_RETURN trên MSP và bảo toàn r4-r11 của task mới.

### 9.1. Build, test và thực thi

Build thật trên repo: O0/O2, CMake + Ninja, Arm GNU GCC 14.2.1,
-Wall -Wextra -Werror. O0 Flash 14212 B, RAM 17500 B; O2 Flash 8808 B,
RAM 17504 B. Linker kiểm tra còn vùng MSP dự trù 1 KiB. Heap còn 3056 B
ở cả hai capture; đây không phải stack high-water mark.

Host harness compile source rta_measure.c thật với fake DWT/UART/kernel:
PASS calibration/search, clipping phần SysTick sau release, counter,
fixed iteration count dưới IRQ mô phỏng và framing đủ 205 JOB2.
Parser tests PASS các trường hợp đủ job, mất job, CAL lặp, CAL dưới budget,
log chưa DONE và counter IRQ sai. ELF checks PASS vector/call structure
cho hai bản. Host/ELF checks không giả lập toàn bộ exception semantics.

Cả hai bản đã nạp bằng OpenOCD/ST-Link với Verified OK, tự reset và thu
qua COM19, 115200 8N1. Log flash và UART có tên measurements/flash_v2_O0/O2.txt và
measurements/trace_v2_O0/O2.csv. Mỗi capture đủ 205 job, chung timestamp cho release
cùng tick, không có deadline miss trong tập đã quan sát. Flash thử đầu
của O2 verify được nhưng không DONE vì direct handler assertion; log
attempt1 được lưu trong lịch sử commit ec693c2, không được tính là
capture đo hợp lệ. File thử lỗi đã được dọn khỏi snapshot nộp cuối.

### 9.2. Calibration DWT

| Task | C mục tiêu (us) | O0 min=max (cycles) | O0 lệch C (us) | O2 min=max (cycles) | O2 lệch C (us) |
| :--- | ---: | ---: | ---: | ---: | ---: |
| tau0 | 9909 | 713450 | 0.027778 | 713453 | 0.069444 |
| tau1 | 10062 | 724466 | 0.027778 | 724469 | 0.069444 |
| tau2 | 31486 | 2266994 | 0.027778 | 2266997 | 0.069444 |
| tau3 | 74591 | 5370554 | 0.027778 | 5370557 | 0.069444 |

Tất cả 8 mẫu mỗi task đạt target <= C_cal <= target + 1 us.
Runtime work wall time vẫn chứa preemption; không dùng nó làm CPU cost.
IRQ không thể rút ngắn công việc vì điều kiện kết thúc là số vòng cố định.

### 9.3. WCRT V2

| Task | RTA danh định (ms) | WCRT O0 (ms) | WCRT O2 (ms) | Số job mỗi bản | Misses O0/O2 |
| :--- | ---: | ---: | ---: | ---: | :--- |
| tau0 | 9.909000 | 10.064958 | 10.008889 | 100 | 0/0 |
| tau1 | 19.971000 | 20.284125 | 20.172389 | 60 | 0/0 |
| tau2 | 51.457000 | 52.221819 | 51.944458 | 30 | 0/0 |
| tau3 | 155.928000 | 158.209444 | 157.401444 | 15 | 0/0 |

V2 timestamp release trực tiếp trong tick hook sau cập nhật kernel tick,
khác release cycle-grid danh định của V1. Vì vậy chênh V1/V2 không chỉ do
optimization mà còn do workload, profiler và định nghĩa mốc đo được làm rõ.

O0: sai lệch actual release so với epoch+k*T nằm trong -9.583333..0.361111 us.

O2: sai lệch actual release so với epoch+k*T nằm trong -3.208333..0.000000 us.

### 9.4. Phân tích sai lệch từ counter đo trong từng response window

| Bản | Task | Delta R (us) | SysTick bracket (us) | PendSV bracket (us) | Residual sau C_cal/IRQ (us) |
| :--- | :--- | ---: | ---: | ---: | ---: |
| O0 | tau0 | 155.958333 | 103.430556 | 15.430556 | 37.069444 |
| O0 | tau1 | 313.125000 | 197.319444 | 32.208333 | 83.541667 |
| O0 | tau2 | 764.819444 | 501.916667 | 66.680556 | 196.138889 |
| O0 | tau3 | 2281.444444 | 1513.069444 | 187.708333 | 580.472222 |
| O2 | tau0 | 99.888889 | 72.041667 | 8.791667 | 18.986111 |
| O2 | tau1 | 201.388889 | 139.125000 | 18.069444 | 44.055556 |
| O2 | tau2 | 487.458333 | 348.888889 | 37.111111 | 101.250000 |
| O2 | tau3 | 1473.444444 | 1066.541667 | 103.819444 | 302.597222 |

Residual đã trừ tổng C_cal của các job đầy đủ góp phần trong cửa sổ, không
chỉ C của riêng task thấp. Analyzer kiểm tra không có carry-in trước khi
áp phép trừ này. Với tau3, counts là (3,2,1,1), đúng lịch RTA.

O0, tau3: 2281.444444 = 1513.069444 (SysTick) + 187.708333 (PendSV) + 0.194444 (C_cal - C danh định) + 580.472222 (residual), đơn vị us. Response window có 159 SysTick segment và 11 PendSV.

O2, tau3: 1473.444444 = 1066.541667 (SysTick) + 103.819444 (PendSV) + 0.486111 (C_cal - C danh định) + 302.597222 (residual), đơn vị us. Response window có 158 SysTick segment và 11 PendSV.

PendSV bracket đã gồm trace hook, scheduler và software save/restore;
không cộng OUT->IN thêm lần nữa. SysTick là nguồn overhead lớn trong
phép đo V2. Residual gồm thời gian ngoài bracket và ngoài workload được
hiệu chỉnh: hardware exception entry/exit, tail chaining, mép wrapper,
code đầu/cuối job và biến thiên timing chưa phân tách. Không gán toàn bộ
residual cho một ISR và không gọi đây là phân rã tuyệt đối từng cycle.

Empty enter/exit probe O0: 70..70 cycles. Đây chỉ là phần bracket rỗng, chưa phải tổng chi phí observer, không tự trừ khỏi mọi IRQ.

Empty enter/exit probe O2: 24..25 cycles. Đây chỉ là phần bracket rỗng, chưa phải tổng chi phí observer, không tự trừ khỏi mọi IRQ.

Kết luận có bằng chứng: C đã được hiệu chỉnh uncontended bằng DWT, công
việc không bị cắt ngắn bởi preemption, và contribution SysTick/PendSV trong
response window đã được đo riêng. RTA và các capture V2 đều không miss
deadline với bộ tham số này. Số đo là maximum hữu hạn, profiler có observer
effect; chưa chứng minh WCRT tuyệt đối hoặc oscillator/timebase calibrated.

### 9.5. Tái lập và phạm vi bằng chứng

Xem V2_GUIDE.md và tests/w1e3/README.md. Analyzer V2 dùng JOB2; parser JOB
của V1 không áp cho format mới. Kết quả JSON giữ cycle gốc để tái tính.
Gantt V2 chỉ bao phủ cửa sổ scheduler ring 254 event; JOB2 đủ 205 job.
Capture LA ở mục 7 thuộc V1. Hai session LA V2 đã được người học thu
riêng sau khi nạp từng bản tương ứng; đối chiếu được ghi ở mục 9.6.

SHA256 ELF/source/log tại thời điểm hoàn tất được lưu trong measurements/V2_MANIFEST.json.
Không push lên remote trong bước này.

### 9.6. Logic Analyzer đối chiếu firmware V2

Người học trực tiếp nạp và thu measurements/capture_v2_O0.sr, capture_v2_O2.sr.
Codex đọc session bằng trace_correlate.py, mapping channels 0,1,2,3,4
tương ứng Idle/tau0/tau1/tau2/tau3 trên PA4..PA8.

| Bản | Event khớp | Sample rate | Samples | Max abs mismatch căn cạnh đầu (us) |
| :--- | ---: | ---: | ---: | ---: |
| V2 O0 | 254 | 1 MHz | 10000000 | 9.305556 |
| V2 O2 | 254 | 1 MHz | 10000000 | 7.500000 |

Cả hai khớp đủ 254 event liên tiếp theo thứ tự tag/IN/OUT; chỉ căn origin
tại cạnh đầu, không fit clock scale hoặc drop event. Mỗi session dài 10 s.
Sai lệch dưới ngưỡng tham chiếu 16 us của W1E2; đây không phải ngưỡng
riêng được sinh cho W1E3. Bước lấy mẫu là 1 us; không suy ra độ chính xác
vật lý tuyệt đối hay hiệu chuẩn oscillator từ các giá trị trên.

UART và LA thu ở các lượt reset riêng, không tuyên bố cùng một lần chạy.
Đối chiếu chỉ bao phủ cửa sổ scheduler ring 254 event; JOB2 vẫn là bằng
chứng đủ 205 job/5H. Kết quả cấu trúc ở measurements/correlation_v2_O0.json và
measurements/correlation_v2_O2.json; hai session mới được giữ riêng với session V1.

### 9.7. Dọn snapshot nộp cuối

Giữ nguyên source, prediction, report, UART CSV, session .sr thành công,
Gantt và JSON/manifest của V1/V2 để tái lập kết quả. Đã dọn Session 1
trùng measurements/capture_v2_O2.sr, các .pvs chỉ chứa trạng thái hiển thị và các file
thu thử lỗi. Bản lỗi vẫn có trong lịch sử Git; dữ liệu của baseline
manifest là snapshot tại thời điểm V1, không phải danh sách bắt buộc của
working tree cuối. .gitignore riêng Ex3 chỉ bỏ qua .pvs, không bỏ qua .sr.


### 9.8. Gom dữ liệu đo

Toàn bộ 16 file dữ liệu/Gantt/kết quả/log và hai manifest đã được chuyển
vào [measurements/](measurements/README.md). Nội dung 16 file được kiểm tra
SHA256 trước và sau khi chuyển; việc đổi thư mục không thay đổi số đo.

Xem [Gantt V2 O0](measurements/gantt_v2_O0.svg) và
[Gantt V2 O2](measurements/gantt_v2_O2.svg). Các lệnh tái phân tích trong
V2_GUIDE.md và tests/w1e3/README.md dùng đường dẫn mới.
measurements/BASELINE_V1_MANIFEST.json giữ nguyên các đường dẫn của snapshot lịch sử;
các CSV/SR V1 còn hiện diện được tìm theo cùng tên trong measurements/.
measurements/V2_MANIFEST.json cập nhật đường dẫn dữ liệu, giữ nguyên các giá trị SHA256.
Prediction và firmware không thay đổi trong bước sắp xếp này.
