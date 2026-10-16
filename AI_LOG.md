W1.1 – AI_LOG

**Sinh viên:** Nguyễn Trọng Nhân  
**MSSV:** 2452882

## Công cụ đã sử dụng

- **Visual Studio Code + CMake:** biên dịch và tạo target W1.1.
- **OpenOCD + ST-Link:** nạp chương trình vào STM32F1 và kiểm tra verify.
- **PulseView + logic analyzer Saleae 8 kênh:** đo waveform TIM và tín hiệu tag GPIO.
- **AI:** hỗ trợ giải thích khái niệm, gợi ý cách kiểm tra lỗi và hướng dẫn đọc kết quả đo.

## Mục đích sử dụng

- Kiểm tra clock/timer có tạo được xung **1 kHz**, duty khoảng **50%**.
- Kiểm tra lịch toggle của task FreeRTOS trên GPIO.
- Xác minh chương trình đã build, flash và chạy đúng trên phần cứng.
- Hỗ trợ hiểu các phần RCC, FLASH, TIM, GPIO, SysTick và PendSV.

## Kết quả tự kiểm chứng

hiện tại em cam kết những phần nào đã thực hiện đc

- OpenOCD nhận diện ST-Link và Cortex-M3.
- Flash thành công, trạng thái **Verified OK**.
- PulseView đo được TIM khoảng **1000 Hz**, chu kỳ **1000 µs**, duty khoảng **50%**.
- Tín hiệu PB3 đo được khoảng **263.936 ms**, phù hợp chu kỳ task A **264 ms**.

## Cam kết

**Kính gửi anh Nguyễn Hoàng Triều và anh Đặng Đức Huy – Mentor,**

Em cam kết sử dụng AI với mục đích hỗ trợ học tập, giải thích khái niệm, gợi ý hướng xử lý lỗi và rà soát cách trình bày báo cáo. AI không được sử dụng để thay thế toàn bộ quá trình tư duy, thiết kế hoặc kiểm chứng bài làm.

Em tự đọc yêu cầu W1.1, tự quyết định cách triển khai, tự thực hiện build bằng CMake, nạp chương trình qua OpenOCD và ST-Link, nối phần cứng, đo waveform bằng logic analyzer và PulseView, sau đó đối chiếu số liệu với dự đoán lý thuyết. Các kết quả như tần số TIM, duty cycle, chu kỳ task và trạng thái `Verified OK` đều được em kiểm tra từ log hoặc phép đo thực tế.

Em không sử dụng CubeMX/CubeIDE wizard, scaffold có sẵn hoặc project mẫu hãng để hoàn thành bài. Em cũng không giao trọn danh sách file cho AI rồi sao chép mà không kiểm tra. Những phần AI gợi ý đều được em đọc lại, điều chỉnh theo cấu trúc của bài và em chịu trách nhiệm về nội dung cuối cùng.

Em cam kết có thể giải thích được các phần quan trọng của chương trình, bao gồm chuỗi startup, linker, cấu hình clock 72 MHz, Flash wait-state, GPIO, TIM tạo xung 1 kHz, SysTick, SVC, PendSV, FreeRTOS scheduler và nguyên nhân của các kết quả đo. Khi được mentor hỏi, em chịu trách nhiệm trình bày được vì sao mã nguồn hoạt động, vì sao số liệu đo phù hợp hoặc sai lệch so với dự đoán, cũng như các giới hạn của phép đo.

**Nguyễn Trọng Nhân – 2452882**

## W1.2 – Tracer và review ngày 15/10/2026

- AI hỗ trợ soạn UART driver, FreeRTOSConfig, tracer/ring buffer,
  hướng dẫn build/flash và phân tích CSV, sigrok session, disassembly
- Người học trực tiếp nối STM32/ST-Link/USB-to-TTL/Logic Analyzer,
  chạy các lệnh, nhấn RESET, thu UART/PulseView và cung cấp dữ liệu
- Claude cung cấp review; Codex đối chiếu lại source và artifact,
  sửa tag/GPIO handling, unwrap lúc dump, interface, preset và REPORT,
  bổ sung script tái lập cùng host regression tests, theo yêu cầu
  “tự sửa tự commit” của người học
- Không chép kết luận review thành số đo mới: báo cáo phân biệt căn
  cạnh đầu với mean fit, phân biệt dữ liệu nhóm với phép đo overhead
  độc lập, không dùng W1.2 để kết luận HSE/LA đã được hiệu chuẩn
- Số đo phần cứng và disassembly hiện lưu thuộc baseline `d673d5f`.
  Firmware sau review đã build/host-check, chưa flash hoặc đo lại;
  không cam kết timing cũ áp dụng cho firmware mới
- PREDICTION giữ nguyên commit trước khi chạy. AI_LOG ghi phạm vi hỗ
  trợ thực tế; khả năng tự giải thích code cần được người học luyện
  và kiểm chứng trong phần vấn đáp, không suy ra từ việc test PASS

## W1.3: RTA và kiểm chứng O0/O2

Ngày: 16/10/2026. Bài: W1E3. Công cụ: Codex.

Yêu cầu: đọc yêu cầu RTA trong repo, giải thích lý thuyết, đưa lệnh sinh
tham số, soạn prediction và code cùng hướng dẫn build/đo từng bước.

AI hỗ trợ sinh bộ tham số bằng tools/gen_params.py cho MSSV 2452882,
kiểm tra utilization/RTA, soạn PREDICTION.md, main.c, rta_app.c,
rta_measure.c, các header/config, cmake/w1e3.cmake, script capture/analysis
và mẫu REPORT.md trong thư mục deliverables bên ngoài repo nộp bài.

Kiểm tra trong phiên chuẩn bị: build/link O0 và O2 bằng Arm GNU GCC 14.2.1
với Werror; API checker PASS trên 8 source/config mới; kiểm tra integer RTA,
ideal RM simulation 5H và parser JOB bằng log synthetic có nhãn rõ ràng.

Phần sinh viên tự đọc và kiểm tra: [CẦN BỔ SUNG].

Sau bước chuẩn bị, người học trực tiếp chạy lệnh build/flash, nối phần cứng,
reset STM32 và thu UART O0/O2 cùng các session PulseView O0/O2. Codex đọc
các artifact và phân tích: đủ 205 job mỗi cấu hình, zero observed deadline
miss; mỗi session LA cuối cùng khớp 254 event UART. Max mismatch căn cạnh
đầu O0 = 7.819444 us, O2 = 8.972222 us ở sample rate 1 MHz. Lần LA O2 đầu
thiếu một cạnh Idle được giữ lại riêng; không sửa/drop event để ép match.

Prediction đã được AI soạn trong deliverables trước các bước đo, nhưng
file trong repo vẫn là template khi kiểm tra sau phép đo. Không có bằng
chứng commit prediction W1E3 trước khi chạy; cần ghi trung thực trình tự.

Selected-time accounting còn chứa một phần ISR/exception, cần hiệu chỉnh C
và định lượng phần overhead chưa bao phủ bởi OUT->IN. Số đo W1E2 baseline
không thay thế số đo mới. WCRT quan sát hữu hạn không phải proof worst case
tuyệt đối; đối chiếu LA/UART ở các lượt reset riêng không hiệu chuẩn clock.

## W1E3 V2: workload hiệu chỉnh và IRQ profiling

Theo yêu cầu thực hiện của người học, Codex chuẩn bị và tích hợp fixed iteration workload, calibration DWT, wrapper ARM_CM3 riêng W1E3, JOB2 parser và prediction trước phép đo mới. Không sửa kernel. Đã giữ checkpoint nguồn và SHA256/ELF/map/CSV/session của V1; không đụng các deletion đang stage trong tests/w1e2. Các số V1 không áp cho firmware V2.

Codex đã trực tiếp mở COM19, nạp/verify/reset qua OpenOCD, thu và phân tích V2 O0/O2. Hai bản đều Verified OK, đủ 205 JOB2, calibration đạt giới hạn đã dự đoán, zero observed miss. Lần nạp thử đầu không DONE do direct handler assertion; đã sửa indirect routing theo port và thêm assert vector riêng. Có counter SysTick/PendSV riêng, residual được báo trung thực; chưa có LA V2. Host/parser/vector checks PASS, không thay số đo V1 bằng V2 và không push remote.

Người học đã thu thêm LA V2 O0/O2; Codex đối chiếu đủ 254 event mỗi bản. Max mismatch căn cạnh đầu O0 = 9.305556 us, O2 = 7.500000 us ở 1 MHz, 10 triệu samples. Giữ session V1/V2 riêng, không drop event hoặc fit clock scale.

## W1E2 – Đo lại và hoàn thiện ngày 16/10/2026

Người học trực tiếp chạy preset w1e2-o0/o2, nạp board và thu lại UART,
PulseView. Codex kiểm tra các file trong measure_20261016_075333 và
chọn capture O2 nguyên vẹn đầu tiên từ log có 13 header: 6 đủ, 7 bị ngắt.
Giữ nguyên UART thô; không ghép dòng hoặc sửa timestamp để ép khớp.
Một template 254-event khớp 12 cụm GPIO đủ, không có ID để gán định
danh từng lần RESET giữa hai thiết bị.

Số đo mới: O0 min/max 438/508, mean 472,841, jitter 70 cycles;
O2 min/max 219/244, mean 232,206, jitter 25 cycles. Max correlation
căn cạnh đầu 1,708333 µs, PendSV mean-fit residual max 0,795619 µs.
Hai build kiểm tra lại thành công, API checker full scope PASS 53 files.

Codex cập nhật REPORT/DONE, tạo waveform PNG từ samples SR thật
(có ghi rõ không phải screenshot), xuất disassembly từ ELF đúng preset,
ghi SHA256/provenance, dọn artifact baseline trùng/cũ và file bố cục PVS.
Không sửa firmware trong bước hoàn thiện nên không làm thay đổi timing
vừa đo. Bộ tests/w1e2 đã được người học yêu cầu xóa, không còn link/lệnh
chạy bộ test đó trong báo cáo cuối. Baseline vẫn có trong lịch sử Git.
Không chỉnh sửa dữ liệu hoặc file tạm của W1E3.

## W1E4 – Hiện thực, đo board và tinh chỉnh stack ngày 16/10/2026

Prompt tiếp nối của người học

> okie tiếp tục làm và hoàn thành các bước ; lưu ý code thì gửi ra màn mình mình tự dán vào

> đã thu xong kiểm tra và tiếp tục

> xong rroif nha

### Phạm vi hỗ trợ của AI

Codex cung cấp nội dung source và lệnh PowerShell trong chat
Người học tự dán code, build, nối phần cứng, nạp firmware và thu UART

Code được hướng dẫn gồm main.c, FreeRTOSConfig.h,
source thí nghiệm heap tạm thời và cấu hình target CMake

Giai đoạn heap dùng heap_4 theo ngoại lệ thí nghiệm trong README
Sau khi lưu đủ log, source heap_experiment.c được bỏ khỏi bản cuối
Các target static không liên kết heap_4

Firmware static dùng xTaskCreateStatic, xQueueCreateStatic,
xSemaphoreCreateMutexStatic cùng callback cấp bộ nhớ Idle và Timer

Có ba target riêng cho Blinky, Trace và RTA
Firmware W1E4 giữ workload cần kiểm tra bộ nhớ,
không giữ buffer JOB2 của phương án prediction ban đầu

Codex hướng dẫn sửa cấu hình PB3 để tắt JTAG và giữ SWD cho bản Blinky

### Hỗ trợ xử lý môi trường

Codex hướng dẫn bổ sung target W1E4 khi CMake chưa nhận target

Codex giải thích lỗi logger thiếu đối số do biến PowerShell chưa có giá trị,
hướng dẫn chọn COM19 và dùng here-string hoàn chỉnh

Khi OpenOCD không đọc được CPU, Codex hướng dẫn người học thử boot ROM
với BOOT0 = 1, BOOT1 = 0 rồi nạp lại
Sau khi nạp phải đưa BOOT0 về 0 để chạy firmware từ Flash

Người học thao tác trực tiếp trên board
Codex không trực tiếp nạp board trong bước kiểm tra cuối

### Kết quả được kiểm tra từ file thật

Log week1/ex4/measurements/heap_log_20261016_192528.csv đủ 72 thao tác qua 6 vòng
Đúng kích thước và thứ tự free của MSSV 2452882

Heap thực chiếm cực đại 872 byte
Minimum-ever thấp nhất 7312 byte
Cuối mỗi vòng free và khối trống lớn nhất đều trở về 8184 byte

Phân mảnh tạm thời xuất hiện khi khối 3 còn ngăn hai vùng trống
Sau khi free khối 3, heap_4 coalescing thành một vùng trống

Sau baseline, stack ứng dụng và Timer giảm từ 256 xuống 128 words
Idle giữ 128 words

Ba log hoàn chỉnh sau tinh chỉnh là

week1/ex4/measurements/static_rta_tuned_20261016_200818.csv
week1/ex4/measurements/static_blinky_tuned_20261016_200954.csv
week1/ex4/measurements/static_trace_tuned_20261016_201024.csv

Cả ba có đúng APP, self-test queue/mutex PASS,
STACK_MARGIN_32_WORDS,1 và W1E4 STATIC DONE

Biên stack thấp nhất là TraceB còn 76 words, tương đương 304 byte

Log RTA và Trace mỗi file có 254 sự kiện tracer,
timestamp không giảm, cặp IN/OUT hợp lệ và overflow=0

RAM section sau tinh chỉnh

w1e4 dùng 6472 byte
w1e4_blinky dùng 3096 byte
w1e4_trace dùng 5192 byte

Bảng tính tay RTA là 6468 byte
Sai số so với map là 4 byte, trong giới hạn 123 byte

RTA tiết kiệm 2560 byte so với baseline
Blinky và Trace mỗi bản tiết kiệm 1536 byte

tools/check_api.sh PASS trên 55 file

Checker được chạy với nội dung script hiện có,
chuẩn hóa xuống dòng trong bộ nhớ và chọn Python đã cài
Không sửa file checker hoặc quy tắc kiểm tra

Đã kiểm tra symbol của cả ba ELF cuối,
không thấy các symbol cấp phát động đã dò
Đã kiểm tra riêng dòng link của từng target W1E4,
không liên kết heap_4 hoặc heap_experiment

git diff --check trả về mã 0

### Dấu vân tay log đã kiểm tra

week1/ex4/measurements/heap_log_20261016_192528.csv
SHA256 132293b3c09e342aed90597603d4e1ed793b41ef8a99368e37f4a19bc5e151c3

week1/ex4/measurements/static_rta_tuned_20261016_200818.csv
SHA256 dec3b48176c36077b44bf183bcb64fb92c05deffab88e2da56a0d6eead4a01e7

week1/ex4/measurements/static_blinky_tuned_20261016_200954.csv
SHA256 77c1d8c52765adb3b11451cfb3fa308c3853592ce865d84b74ebb98a11fd9396

week1/ex4/measurements/static_trace_tuned_20261016_201024.csv
SHA256 82b2d388554c0e62530fc896c977d2d9c6a54df79307b74bccb9c05649512bc8

### Giới hạn và trách nhiệm kiểm chứng

Giữ nguyên log NUL và các log thiếu đầu,
không dùng chúng thay cho log hoàn chỉnh

High-water mark chỉ phản ánh những đường chạy đã thực thi
Calibration không thay cho phép đo WCRT hoặc deadline

Chưa có kiểm thử chạy kéo dài hoặc phép đo logic analyzer riêng của W1E4
Không khẳng định mọi tình huống đều an toàn stack chỉ từ các lượt đo này

PREDICTION.md giữ lịch sử thiết kế ban đầu và phần điều chỉnh
Không có commit prediction trước phép đo theo yêu cầu không commit của người học

Codex cung cấp REPORT.md và phần AI_LOG này trong chat để người học tự dán
Không tự sửa hai file trong bước kiểm tra cuối
Không commit hoặc push

Phần người học tự giải thích lại về coalescing,
high-water mark và cách đọc map [CẦN BỔ SUNG]

## W1E4 – Gom log đo trước khi người học commit

Prompt của người học

> gôm file static và heap_log vào 1 thư mục cho gọn sau đó đưa lệnh mình commit

Codex gom 9 log CSV static và heap_log vào week1/ex4/measurements
Giữ nguyên tên và nội dung, gồm log hoàn chỉnh, baseline và log lỗi hoặc thiếu đầu
Đối chiếu SHA256 trước và sau khi chuyển để kiểm tra dữ liệu không đổi

Cập nhật đường dẫn trong REPORT.md và AI_LOG.md
Chuẩn hóa tên header FreeRTOSconfig.h thành FreeRTOSConfig.h,
không đổi nội dung header

Không tạo bản sao log hoặc file manifest mới
Không stage, commit hoặc push, chỉ cung cấp lệnh để người học tự chạy

## W1E4 – Dọn hai lượt thu không cần giữ

Người học yêu cầu rà Week 1 và cung cấp lệnh tự xóa

Codex kiểm tra file, link tài liệu, trạng thái Git và API checker
API checker PASS 55 file

Người học chạy lệnh loại static_rta_20261016_194218.csv vì toàn byte NUL
và static_blinky_tuned_20261016_200901.csv vì trùng phần dữ liệu
với bản hoàn chỉnh static_blinky_tuned_20261016_200954.csv

Các file này trước đó nằm trong week1/ex4/measurements
Đã cập nhật REPORT để phản ánh việc loại bỏ
Giữ nguyên log heap, baseline còn lại và ba log tuned hoàn chỉnh
Giữ dữ liệu W1E2 và các bộ bằng chứng V1/V2 của W1E3

Codex cung cấp lệnh, không trực tiếp xóa, commit hoặc push trong lượt này
