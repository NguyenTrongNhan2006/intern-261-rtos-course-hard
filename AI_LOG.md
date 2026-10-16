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
