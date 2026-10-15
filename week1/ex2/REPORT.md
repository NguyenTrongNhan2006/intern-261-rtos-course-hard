# W1.2 – REPORT

- MSSV: 2452882
- Ngày đo baseline: 15/10/2026
- MCU: STM32F103C8, Cortex-M3, HCLK = 72 MHz
- Toolchain: Arm GNU GCC 14.2.1, `-Wall -Wextra -Werror`
- PREDICTION commit trước khi chạy: `3fdd85c`
- Firmware đã đo và disassembly đính kèm: **baseline `d673d5f`**

## Số đo

**Phân biệt baseline và firmware sau review**

Toàn bộ số đo phần cứng trong báo cáo này thuộc baseline `d673d5f`.
Bản sau review thay cách cập nhật GPIO, giới hạn tag và phạm vi helper;
compiler có thể inline hoặc tạo code khác, nên phải nạp và đo lại trước
khi áp các con số này cho firmware mới. Hiện bản mới đã build và kiểm
thử logic trên host, **chưa flash/đo lại trên STM32**.

Giữ nguyên CSV, `.sr`, ảnh và disassembly baseline để có thể kiểm tra
kết quả cũ. Không ghi đè chúng bằng disassembly của firmware mới.

**Cấu hình thí nghiệm baseline**

- Hai cấu hình đều là `Debug`, lần lượt override flags `-O0 -g3` và `-O2 -g3`
- FreeRTOS tick rate: 1000 Hz, tương ứng 1 ms/tick
- Task A: tag 1, priority 1, busy-wait 200 µs, period 2 ms
- Task B: tag 2, priority 1, busy-wait 300 µs, period 3 ms
- Idle: tag 0, priority 0, không có tag callback
- UART1: PA9 TX, 115200 baud, 8N1
- Logic Analyzer: PulseView, sample rate 1 MHz, 10 triệu samples trong 10 giây
- GPIO HIGH tại hook IN, LOW tại hook OUT

| Task | Tag | GPIO | Logic Analyzer |
| --- | ---: | --- | --- |
| Idle | 0 | PA4 | D0 |
| Task A | 1 | PA5 | D1 |
| Task B | 2 | PA6 | D2 |

Tracer đọc DWT tại đầu mỗi hook, sau đó lấy tag, ghi GPIO và static
ring buffer. Buffer 256 slots giữ một slot để phân biệt đầy/rỗng,
chủ động dừng sau OUT trước khi hết chỗ, nên mỗi capture có 254 records,
`overflow=0`, đầu IN và cuối OUT. UART chỉ dump khi recording đã dừng,
không blocking UART hoặc cấp phát bộ nhớ trong hook.

**Phạm vi phép đo**

Đo `timestamp_IN_kế_tiếp - timestamp_OUT`, không phải toàn bộ latency
context switch. Khoảng này gồm phần thân hook OUT sau timestamp,
phần scheduler giữa hai hook, call/prologue hook IN đến timestamp.
Nó không bao phủ toàn bộ exception entry/exit và PendSV save/restore.
Vùng HIGH cũng có thể chứa ISR, không phải exclusive task CPU time.

| Optimization | min (cycle) | max (cycle) | jitter = max-min |
| --- | ---: | ---: | ---: |
| -O0 | 462 | 534 | 72 |
| -O2 | 264 | 290 | 26 |

Mỗi cấu hình có 126 mẫu OUT → next IN.

| Optimization | min (µs) | max (µs) | mean (cycle) | mean (µs) |
| --- | ---: | ---: | ---: | ---: |
| -O0 | 6,417 | 7,417 | 497,952 | 6,916 |
| -O2 | 3,667 | 4,028 | 278,214 | 3,864 |

**Gantt và ảnh đo**

![Gantt baseline -O0](gantt_O0.svg)

![Gantt baseline -O2](gantt_O2_la.svg)

D0 = PA4 = Idle, D1 = PA5 = A, D2 = PA6 = B:

![Logic Analyzer tổng quan](logic_O2_overview.png)

![Task A, hai cursor tại IN và OUT, khoảng 211 µs](logic_O2_taskA.png)

Các đoạn ba kênh HIGH trước khi GPIO được khởi tạo không thuộc trace.
Sau reset GPIO chưa được cấu hình output; không diễn giải các đoạn này
thành ba task cùng chạy, cũng không dùng làm mốc căn thời gian.

**Dữ liệu và tái lập phân tích**

- [CSV -O0](trace_O0.csv), [CSV -O2](trace_O2.csv)
- [UART phiên LA gồm bốn capture](trace_O2_la.csv)
- [Capture UART đầu tiên đã tách](trace_O2_la_single.csv)
- [Session Logic Analyzer gốc](capture_O2.sr)
- [Disassembly baseline -O0](disassembly_O0.txt), [-O2](disassembly_O2.txt)
- [Kết quả đối chiếu có cấu trúc](correlation_O2.json)
- [Script đối chiếu](../../tools/trace_correlate.py)

Chạy từ repo root với Python 3, không cần package ngoài:

```text
python tools/trace_gantt.py week1/ex2/trace_O0.csv --freq-hz 72000000
python tools/trace_gantt.py week1/ex2/trace_O2.csv --freq-hz 72000000 --params-h week1/ex2/include/params.h
python tools/trace_correlate.py week1/ex2/capture_O2.sr week1/ex2/trace_O2_la.csv
python tools/trace_correlate.py week1/ex2/capture_O2.sr week1/ex2/trace_O2_la.csv --json
```

`trace_correlate.py` đọc sigrok v2, kiểm tra thứ tự UART và cặp IN/OUT
trước khi đối chiếu. Không sort dữ liệu lỗi, không fit hệ số clock scale;
exit code 0 là đạt ngưỡng căn cạnh đầu, 1 là vượt ngưỡng, 2 là dữ liệu lỗi.
Công cụ chỉ nhận digital samples một byte và tag có GPIO mapping.

Giữ các artifact ban đầu và các bản tách để bảo toàn quá trình đo:
`trace_initial.csv` giống `trace_O0.csv`, `gantt_initial.svg` giống
`gantt_O0.svg`. Các bản CSV -O2 có cùng records sau khi bỏ comment/
blank line; `gantt_O2_la.svg` giống `gantt_O2.svg`. Không xem các file
trùng này là các thí nghiệm độc lập.

**Tái lập build và kiểm tra bản sửa**

Preset mới tách thư mục build khỏi ELF baseline cũ:

```text
cmake --preset w1e2-o0
cmake --build --preset w1e2-o0
cmake --preset w1e2-o2
cmake --build --preset w1e2-o2
python -B -m unittest discover -s tests/w1e2 -p "test_*.py" -v
```

Hai preset dùng `Debug`, flags chính xác `-O0 -g3` / `-O2 -g3`.
Không dùng mặc định `Release` vì mức tối ưu mặc định có thể khác.
Output ở `build/w1e2-current-O0` và `build/w1e2-current-O2`.

Bản sửa build sạch cả hai cấu hình. C host harness chạy source
`tracer.c` thật với fake registers/UART, đã kiểm tra tag ngoài mapping,
đổi GPIO range, lựa chọn getter thread/ISR, freeze/dump và hai lần wrap.
Host test kiểm tra logic/framing, không xác nhận timing hoặc bus ordering
trên Cortex-M3. Xem [lệnh chạy host test](../../tests/w1e2/README.md).

API checker full scope mặc định `platform week1 week2 week3 week4`:

```text
check_api: PASS (47 files)
```

Script Bash dùng Python 3 để lọc comment. Trên Windows cần Git Bash,
Python 3 hợp lệ và xử lý CRLF khi chạy script. PASS API không thay thế
build, flash hoặc phép đo phần cứng.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

**1. Căn thời gian và sai số Gantt / Logic Analyzer của baseline**

Có bốn lần capture, mỗi lần khớp đủ 254 events, không lệch tag hoặc
chiều cạnh trong cửa sổ trace. Bốn đoạn UART giống hệt nhau về cycles,
tag và event, cho thấy baseline lặp lại được trong cấu hình này.

Phép so sánh bảo thủ lấy IN đầu tiên của Task B làm mốc:

```text
t_DWT(i) = (cycles_i - cycles_0) / 72                [µs]
t_LA(i)  = (sample_i - sample_0) / 1 MHz × 10^6      [µs]
d_i      = t_LA(i) - t_DWT(i)
```

Chỉ dịch origin, không thay tỷ lệ trục thời gian.

| Capture | max abs(d_i) khi căn cạnh đầu (µs) |
| --- | ---: |
| 1 | 1,889 |
| 2 | 1,472 |
| 3 | 1,653 |
| 4 | 1,111 |

Ngưỡng `W1E2_CORRELATION_TOLERANCE_US = 16 µs` đạt ngay với cách
căn này: max khoảng 1,89 µs. Đây vẫn là tiêu chí PASS của script.

Ví dụ capture đầu, origin LA tại 3035374 µs:

| Event | Tracer tương đối (µs) | LA tương đối (µs) | abs(d_i) (µs) |
| --- | ---: | ---: | ---: |
| Task B OUT đầu tiên | 316,583 | 316 | 0,583 |
| Event 128 | 42328,500 | 42327 | 1,500 |
| Event 254 | 84328,500 | 84327 | 1,500 |

Event đầu gọi từ `vTaskStartScheduler` ở thread mode; các event còn lại
được gọi trong PendSV. Hai đường lấy tag dùng critical section khác
nhau nên latency timestamp → GPIO không nhất thiết bằng nhau.
Một cạnh mốc còn mang pha lượng tử của sample 1 µs.

Để tách offset hằng khỏi độ phân tán, tính thêm fit mean trên **253
events trong PendSV**, loại event đầu ra khỏi tập fit và thống kê:

```text
offset = mean(d_i), i = 1..253
e_i = d_i - offset
```

| Capture | Mean d_i PendSV (µs) | SD PendSV (µs) | max abs(e_i) PendSV (µs) | Residual event thread đầu (µs) |
| --- | ---: | ---: | ---: | ---: |
| 1 | -1,079 | 0,378 | 0,810 | 1,079 |
| 2 | -0,810 | 0,231 | 0,662 | 0,810 |
| 3 | -0,881 | 0,290 | 0,772 | 0,881 |
| 4 | -0,268 | 0,406 | 0,843 | 0,268 |

Nếu fit mean trên cả 254 events, max residual lần lượt là 1,074;
0,807; 0,878; 0,844 µs. Vì vậy phải ghi rõ phạm vi fit, không gọi
0,84 µs là sai số lớn nhất trên toàn bộ events sau mọi cách căn.
Sai số có dấu cũng không luôn âm: capture 4 có event d_i khoảng +0,417 µs.

Sampling 1 MHz tương ứng 1 µs/sample. Residual sau fit phù hợp với
lượng tử hóa và biến thiên timestamp → GPIO, nhưng không chứng minh
độ chính xác vật lý tuyệt đối dưới 1 µs, cũng không hiệu chuẩn HSE
hoặc timebase Logic Analyzer. Không suy ra clock drift chính xác từ
một cửa sổ khoảng 84 ms và một vài cạnh.

**2. Assembly và chênh lệch -O0 / -O2 của baseline**

Mean giảm 497,952 → 278,214 cycles, giảm khoảng 219,738 cycles,
3,052 µs hoặc 44,1%.

| Hàm baseline | -O0 (bytes) | -O2 (bytes) |
| --- | ---: | ---: |
| `gpio_set` | 38 | 12 |
| `tracer_on_switched_out` | 60 | 44 |
| `vTaskSwitchContext` | 180 | 128 |

Đây là kích thước code, không phải cycle count.
`-O0` tạo stack frame, lưu rồi đọc lại timestamp/tag từ stack;
`-O2` giữ timestamp trong `r4`, giảm truy cập stack và dùng tail call
`b.w tracer_record` ở cuối hook. GPIO helper cũng giảm stack access.

Đường OUT → IN chứa phần thân hook OUT, gồm lấy application tag,
`FromISR` critical section với BASEPRI, callback tag khi không NULL,
GPIO, buffer write, DMB và tính free slots. `DMB` là memory barrier;
CMSIS intrinsic cũng có compiler memory clobber, không chỉ là NOP
hoặc phép thay thế cho giả định một producer.

Các lệnh chính save/restore register của PendSV giữ nguyên.
Không biến số lệnh hoặc kích thước code thành cycle count khi chưa
xét pipeline, branch, Flash wait state và bus access.

**3. Jitter theo từng đường chuyển task**

| Hướng switch baseline | Số mẫu | -O0 min–max (cycles) | -O2 min–max (cycles) |
| --- | ---: | ---: | ---: |
| Idle → A | 28 | 462 | 264 |
| Idle → B | 28 | 462 | 264 |
| B → A | 14 | 498 | 288 |
| A → Idle | 42 | 528–534 | 289–290 |
| B → Idle | 14 | 534 | 290 |

Với -O2, jitter tổng = 290 - 264 = 26 cycles ≈ 0,361 µs,
nhỏ hơn `W1E2_CTXSW_JITTER_MAX_CYCLES_O2 = 355` → PASS baseline.

So B → A với Idle → A: chênh 24 cycles ở -O2 và 36 ở -O0.
Disassembly `tracer_current_tag` xác nhận Idle có callback NULL nên
return sớm, còn task A/B gọi gián tiếp callback và kiểm tra tag.
Đây là bằng chứng tracer tạo đường xử lý phụ thuộc tag.

So B → Idle với B → A: chênh 2 cycles ở -O2 và 36 ở -O0.
Phần chọn priority có đường tìm xuống Idle và đường giữ priority 1;
ready-list rotation cũng có nhánh kiểm tra end marker.

Về số học, độ trải 26 = 24 + 2 cycles trong các nhóm quan sát.
24 cycles chiếm khoảng 92,3% độ trải đó. Đây là phân tích theo nhóm,
không phải phép đo độc lập chứng minh chính xác 24 cycles thuộc tracer
và 2 cycles chỉ thuộc kernel: cùng số vòng tìm priority không đảm bảo
mọi nhánh ready-list hoặc trạng thái bus giống nhau. Muốn tách tuyệt
đối phải timestamp thêm cuối hook OUT, hoặc đo các biến thể kiểm soát.

Không gán riêng 2 cycles cho Flash wait state chỉ từ CSV/disassembly.

**4. PREDICTION và giới hạn task set**

| Đại lượng baseline | Dự đoán trước khi chạy | Thực tế |
| --- | ---: | ---: |
| -O0 min/max | 300/700 cycles | 462/534 cycles |
| -O2 min/max | 120/350 cycles | 264/290 cycles |
| -O2 jitter | 230 cycles | 26 cycles |
| Max sai lệch căn cạnh đầu | 4 µs | Khoảng 1,89 µs |

PREDICTION đã tính scheduler và instrumentation vào phạm vi đo.
Sai lệch không phải do quên phạm vi đó, mà do budget từng đường chưa
được xác lập từ implementation: min -O0 cao hơn dự đoán 162 cycles,
min -O2 cao hơn 144 cycles. Các getter, tag callback, GPIO, buffer,
barrier và stack traffic thực tế nặng hơn ước lượng ban đầu.
Chênh theo tag quan sát được riêng là 36 / 24 cycles, chưa giải thích
hết toàn bộ phần budget thiếu nên không quy hết 144 cycles cho nó.

Jitter nhỏ hơn dự đoán trong baseline có các điều kiện:

- Chỉ priority 0 và 1, chỉ khảo sát các đường tìm priority tương ứng
- Trong khoảng OUT → IN, port đã đặt BASEPRI = 0x50, priority threshold 5;
  SysTick priority 15 bị mask, không chen vào cửa sổ này
- Không có tải ISR priority cao được chủ động thêm vào; NMI/HardFault
  và IRQ priority cao hơn ngưỡng vẫn không bị BASEPRI này che
- Bốn UART capture lặp lại giống hệt nhau trong task set đã đo

Chỉ có năm hướng switch ở bảng trên. Chưa khảo sát khoảng cách
priority ≥ 2, chuyển task do time slicing hoặc yield từ ISR.
Max 290 cycles không phải worst-case bound cho task set khác.

PREDICTION giả định LA 24 MHz; thực tế dùng 1 MHz. Báo cáo giữ nguyên
prediction commit cũ và ghi rõ cấu hình đo thực tế.

## Trả lời câu hỏi trade-off

Tick 1 ms quyết định Ready/release time, không lượng tử hóa timestamp
DWT: một cycle danh định khoảng 13,89 ns. Khoảng OUT → IN đo được
3,667–4,028 µs không phải bội số 1 ms.

Trong các đường quan sát, tag lookup của tracer là nguồn chênh lệch
có bằng chứng cụ thể: Idle không callback, task có callback; hai nhóm
chênh 24 cycles ở -O2, so với jitter tổng 26 cycles. Vì vậy không được
trình bày toàn bộ jitter như biến thiên chi phí kernel thuần.

GPIO update và buffer write đều xâm nhập phép đo. Chưa có ablation
để xếp hạng riêng GPIO và ring-buffer overhead; 44,1% mean giảm khi
optimization thay đổi cũng gồm scheduler và các helper. Phân tích
định lượng theo nhóm ở trên là bằng chứng hiện có, không thay thế
một phép đo tách thành phần.

Static ring buffer và UART dump sau freeze giúp giới hạn nhiễu UART,
đổi lại chỉ giữ một cửa sổ hữu hạn. Việc bỏ GPIO helper call và làm
helper private trong bản sửa làm thay đổi code sinh ra, nên phải đo
lại trước khi dùng timing mới cho W1.3 hoặc kết luận đạt rubric.

**Hành vi firmware sau review**

- `tracer.h` trở về interface khóa học gốc, không expose producer/stop helper
- Một producer hợp lệ: hook thread đầu tiên rồi PendSV; helper ghi record là static
- GPIO mask và tag range suy từ `TRACER_FIRST_PIN/LAST_PIN`; pin range hợp lệ 0..15
- Tags ngoài số GPIO vẫn lưu UART, bỏ GPIO update, không assert trong hook
- NULL callback quy ước Idle tag 0; callback 1..254 hợp lệ, giá trị âm hoặc
  ≥255 thành marker 255. Callback do ứng dụng cung cấp phải nhanh và không blocking
- GPIO trong hook ghi trực tiếp BSRR/BRR, không đổi sampling point DWT
- Record trong RAM vẫn 32-bit để giữ chi phí hook; dump mở rộng sang 64-bit
  bằng cộng 2^32 khi timestamp thô giảm. CSV vẫn là `cycles,tag,IN|OUT`
- Điều kiện unwrap: khoảng giữa hai records liên tiếp **nhỏ hơn**
  2^32 cycles, khoảng 59,65 s tại 72 MHz; số wrap trước record đầu không biết.
  Nếu khoảng cách ≥ một vòng counter thì không thể khôi phục số vòng chỉ
  bằng CYCCNT, cần counter khác hoặc cơ chế lấy mẫu bổ sung

Host test đã kiểm tra hai wrap quan sát được, không phải bằng chứng
đã chạy firmware mới 120 giây trên phần cứng.

**Nguồn tham chiếu**

- [sigrok session v2](https://sigrok.org/wiki/File_format:Sigrok/v2)
- [CMake presets v2 / CMake 3.20](https://cmake.org/cmake/help/v3.20/manual/cmake-presets.7.html)
- [CMSIS Core register access / BASEPRI](https://arm-software.github.io/CMSIS_5/Core/html/group__Core__Register__gr.html)
