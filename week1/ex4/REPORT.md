# W1.4 – Cấp phát tĩnh và ngân sách RAM

MSSV 2452882
Ngày đo 16/10/2026
Board STM32F103, RAM 20 KiB
Build CMake + Ninja, cấu hình Debug với -O2 -g3
Bật -Wall -Wextra -Werror

## Tham số thí nghiệm

Tham số lấy từ params.h và PARAMS.md của MSSV 2452882

| Chỉ số khối | Payload byte | Byte heap thực chiếm |
| --- | ---: | ---: |
| 0 | 320 | 328 |
| 1 | 128 | 136 |
| 2 | 56 | 64 |
| 3 | 24 | 32 |
| 4 | 200 | 208 |
| 5 | 96 | 104 |
| Tổng | 824 | 872 |

Thứ tự cấp phát là 0 1 2 3 4 5

Thứ tự giải phóng là 5 4 2 1 0 3

Thực hiện 6 vòng, mỗi vòng gồm 6 lần cấp phát và 6 lần giải phóng
Tổng cộng 72 thao tác

Sai số RAM cho phép là 123 byte

## Log phân mảnh heap_4

Log đầy đủ được giữ nguyên tại
[measurements/heap_log_20261016_192528.csv](measurements/heap_log_20261016_192528.csv)

Đã kiểm tra đủ 72 dòng HEAP, đúng thứ tự thao tác và đủ 6 vòng
Log kết thúc bằng `# W1E4 HEAP DONE`

Heap cấu hình 8192 byte
Free heap ban đầu đo được là 8184 byte
Minimum-ever ở dòng BASE là 8168 byte do bước cấp phát thử trước chuỗi chính

Bảng dưới ghi vòng đầu, cycle 0

| Bước | Thao tác | Khối | Free hiện tại | Minimum-ever | Khối trống lớn nhất | Số khối trống |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1 | ALLOC | 0 | 7856 | 7856 | 7856 | 1 |
| 2 | ALLOC | 1 | 7720 | 7720 | 7720 | 1 |
| 3 | ALLOC | 2 | 7656 | 7656 | 7656 | 1 |
| 4 | ALLOC | 3 | 7624 | 7624 | 7624 | 1 |
| 5 | ALLOC | 4 | 7416 | 7416 | 7416 | 1 |
| 6 | ALLOC | 5 | 7312 | 7312 | 7312 | 1 |
| 7 | FREE | 5 | 7416 | 7312 | 7416 | 1 |
| 8 | FREE | 4 | 7624 | 7312 | 7624 | 1 |
| 9 | FREE | 2 | 7688 | 7312 | 7624 | 2 |
| 10 | FREE | 1 | 7824 | 7312 | 7624 | 2 |
| 11 | FREE | 0 | 8152 | 7312 | 7624 | 2 |
| 12 | FREE | 3 | 8184 | 7312 | 8184 | 1 |

Các vòng cycle 1 đến 5 lặp lại cùng giá trị free hiện tại,
khối trống lớn nhất và số khối trống tại từng thao tác
Minimum-ever giữ nguyên 7312 byte

Cuối cả 6 vòng đều có free hiện tại 8184 byte,
khối trống lớn nhất 8184 byte và một khối trống duy nhất

## Giải thích phân mảnh định lượng

Tổng payload là 824 byte
Sáu khối có thêm tổng cộng 48 byte header,
nên heap thực chiếm cực đại 872 byte

Kết quả khớp phép tính

8184 - 872 = 7312 byte

Sau khi free khối 5 và 4, vùng được giải phóng nối với vùng trống cuối heap
Allocator gộp chúng thành một khối lớn

Sau khi free khối 2, khối 3 vẫn còn được cấp phát
Vì vậy vùng trống phía trước và phía sau khối 3 chưa thể gộp lại

Sau khi free tiếp khối 1 và 0, số khối trống vẫn là 2
Ở bước 11, tổng free là 8152 byte nhưng khối trống lớn nhất chỉ có 7624 byte
Đây là phân mảnh ngoài tạm thời

Sau khi free khối 3, các vùng trống liền kề được gộp lại
Free hiện tại và khối trống lớn nhất cùng trở về 8184 byte

Có 3 trạng thái phân mảnh trong mỗi vòng, tổng cộng 18 dòng trong 6 vòng
Không còn phân mảnh tồn dư ở cuối mỗi vòng trong phép thử này

Minimum-ever vẫn giữ 7312 byte sau khi đã free hết
Đây là mức free thấp nhất từng ghi nhận, không phải lượng RAM còn bị chiếm
Không thể dùng riêng minimum-ever để kết luận rò rỉ bộ nhớ

heap_4 dùng first-fit trên danh sách khối trống được sắp theo địa chỉ
Coalescing gộp các khối liền kề theo địa chỉ bộ nhớ
Cụm best-fit trong PARAMS.md không được dùng để giải thích thuật toán

Kết quả phù hợp dự đoán về phân mảnh tạm thời và phục hồi F0
Kết luận này áp dụng cho chuỗi 72 thao tác đã đo

## Chuyển sang cấp phát tĩnh

Firmware cuối dùng configSUPPORT_DYNAMIC_ALLOCATION = 0
và configSUPPORT_STATIC_ALLOCATION = 1

Task được tạo bằng xTaskCreateStatic
Queue được tạo bằng xQueueCreateStatic
Mutex được tạo bằng xSemaphoreCreateMutexStatic

Ứng dụng cung cấp bộ nhớ Idle và Timer qua
vApplicationGetIdleTaskMemory và vApplicationGetTimerTaskMemory

Có ba target kiểm tra riêng

| Target | APP | Task ứng dụng |
| --- | ---: | --- |
| w1e4_blinky | 1 | BlinkA, BlinkB |
| w1e4_trace | 2 | TraceA, TraceB |
| w1e4 | 3 | RTA0, RTA1, RTA2, RTA3 |

Mỗi target còn có Idle task và Timer daemon
Ba target là ba firmware độc lập, không cộng RAM của chúng với nhau

Source thí nghiệm heap_experiment.c đã được bỏ khỏi bản cuối sau khi lưu log
Các target W1E4 cuối không liên kết heap_4 hoặc heap_experiment

Cấu hình build cuối nằm trong build/w1e4-tuned
W1E4_HEAP_STAGE trong cache là OFF

tools/check_api.sh PASS trên 55 file

Kiểm tra symbol của cả ba ELF cuối không thấy pvPortMalloc,
vPortFree, ucHeap hoặc các API tạo đối tượng động đã kiểm tra

Cả ba log cuối đều ghi QUEUE_MUTEX_SELFTEST,PASS

## Đo và tinh chỉnh stack

Baseline dùng stack ứng dụng 256 words và Timer 256 words
Idle dùng 128 words

Sau khi đọc high-water mark, giảm stack ứng dụng và Timer xuống 128 words
Idle giữ nguyên 128 words

Trên cấu hình này, một StackType_t chiếm 4 byte
Mỗi stack 128 words tương đương 512 byte

Mức kiểm tra đặt ra là còn ít nhất 32 words, tương đương 128 byte

Các số dưới đây lấy từ log sau tinh chỉnh
Used là depth trừ high-water mark, không phải số đo stack tức thời

| Target | Task | Depth words | Free words | Used words | Free byte |
| --- | --- | ---: | ---: | ---: | ---: |
| RTA | RTA0 | 128 | 98 | 30 | 392 |
| RTA | RTA1 | 128 | 98 | 30 | 392 |
| RTA | RTA2 | 128 | 98 | 30 | 392 |
| RTA | RTA3 | 128 | 98 | 30 | 392 |
| RTA | Idle | 128 | 80 | 48 | 320 |
| RTA | Timer | 128 | 96 | 32 | 384 |
| Blinky | BlinkA | 128 | 100 | 28 | 400 |
| Blinky | BlinkB | 128 | 100 | 28 | 400 |
| Blinky | Idle | 128 | 96 | 32 | 384 |
| Blinky | Timer | 128 | 96 | 32 | 384 |
| Trace | TraceA | 128 | 98 | 30 | 392 |
| Trace | TraceB | 128 | 76 | 52 | 304 |
| Trace | Idle | 128 | 88 | 40 | 352 |
| Trace | Timer | 128 | 96 | 32 | 384 |

Task còn ít stack nhất là TraceB với 76 words, tương đương 304 byte
Giá trị này lớn hơn ngưỡng 32 words

Cả ba log đều có STACK_MARGIN_32_WORDS,1

High-water mark chỉ phản ánh các đường chạy đã thực thi
Kết quả chưa chứng minh mức dùng stack lớn nhất cho mọi tình huống tương lai

## Bằng chứng UART sau tinh chỉnh

[RTA sau tinh chỉnh](measurements/static_rta_tuned_20261016_200818.csv)

[Blinky sau tinh chỉnh](measurements/static_blinky_tuned_20261016_200954.csv)

[Trace sau tinh chỉnh](measurements/static_trace_tuned_20261016_201024.csv)

Cả ba file có đúng header APP tương ứng, kết quả self-test,
bảng stack và marker W1E4 STATIC DONE
Không có byte NUL

Log RTA và Trace mỗi file có 254 sự kiện tracer
Timestamp không giảm, các cặp IN và OUT đúng thứ tự và cùng tag
Cả hai ghi overflow=0

Các mẫu calibration ghi nhận thời gian vòng lặp bằng target cộng 5 cycle
Đây là kết quả theo DWT và cấu hình clock của firmware
Không dùng kết quả này thay cho phép đo WCRT hoặc deadline của W1E3

## Ngân sách RAM tính tay

Bảng dưới áp dụng cho target w1e4 APP 3 sau tinh chỉnh

sizeof StaticTask_t là 96 byte
sizeof StaticQueue_t là 80 byte

| Nhóm đối tượng | Tính tay byte |
| --- | ---: |
| Stack bốn task ứng dụng, 4 × 128 × 4 | 2048 |
| Stack Idle, 128 × 4 | 512 |
| Stack Timer, 128 × 4 | 512 |
| Sáu TCB, 6 × 96 | 576 |
| Handle task và chỉ số task | 32 |
| Mảng high-water mark | 24 |
| Queue, mutex, storage và handle | 184 |
| Calibration | 64 |
| Trạng thái report, hyperperiod và completed | 12 |
| Tracer | 2064 |
| SystemCoreClock | 4 |
| Biến scheduler | 236 |
| Bộ nhớ kernel timer | 188 |
| Biến port và padding đã dự toán | 12 |
| Tổng section RAM tính tay | 6468 |

Queue và mutex gồm hai control block 80 byte,
storage queue 16 byte và hai handle tổng 8 byte

## Đối chiếu với map thật

Map dùng để đối chiếu là build/w1e4-tuned/w1e4.map

| Section | Tính tay byte | Map byte | Map trừ tính tay |
| --- | ---: | ---: | ---: |
| .data | 8 | 8 | 0 |
| .bss | 6460 | 6464 | 4 |
| .noinit | 0 | 0 | 0 |
| Tổng section RAM | 6468 | 6472 | 4 |

Map có tổng cộng 7 byte fill trong .bss
Bảng tính tay đã dự toán 3 byte padding
Phần chênh thêm 4 byte xuất hiện do căn chỉnh các mảng stack

Sai số tổng là 4 byte, nhỏ hơn giới hạn 123 byte
Kết quả đối chiếu đạt yêu cầu sai số

Các stack và buffer trong bảng đối tượng đã nằm trong .bss
Không cộng lại chúng lần nữa lên tổng .data + .bss

Linker còn dành riêng 1024 byte cho MSP

| Thành phần | Tính tay byte | Thực tế hoặc dự trữ byte |
| --- | ---: | ---: |
| Section RAM | 6468 | 6472 |
| MSP dự trữ | 1024 | 1024 |
| Tổng gồm MSP | 7492 | 7496 |

MSP là vùng dự trữ qua giới hạn linker, không phải một section .bss bổ sung
Dung lượng còn lại sau section RAM và MSP là 20480 - 7496 = 12984 byte

## RAM trước và sau tinh chỉnh

| Target | RAM baseline byte | RAM sau tinh chỉnh byte | Tiết kiệm byte | Sau tinh chỉnh gồm MSP byte |
| --- | ---: | ---: | ---: | ---: |
| w1e4 | 9032 | 6472 | 2560 | 7496 |
| w1e4_blinky | 4632 | 3096 | 1536 | 4120 |
| w1e4_trace | 6728 | 5192 | 1536 | 6216 |

Bản RTA giảm bốn stack ứng dụng và một stack Timer
Mỗi stack giảm 128 words, tương đương 512 byte
Tổng tiết kiệm là 5 × 512 = 2560 byte

Blinky và Trace mỗi bản giảm hai stack ứng dụng và một stack Timer
Tổng tiết kiệm là 3 × 512 = 1536 byte

## Đối chiếu lịch sử prediction

PREDICTION.md giữ nguyên dự toán thiết kế ban đầu 15888 byte
Phương án đó có buffer JOB2, khác firmware W1E4 hiện tại

Phương án tính tay baseline 9028 byte đã được cung cấp trong hướng dẫn
nhưng chưa được cập nhật vào file prediction trước lượt baseline

Phần điều chỉnh thiết kế trong PREDICTION.md ghi lại việc giảm stack,
dự toán mới 6468 byte và dự kiến map RTA 6472 byte

Map cuối đúng 6472 byte
Biên stack sau tinh chỉnh đạt mức kiểm tra đã đặt ra

Không có commit prediction trước phép đo do người học yêu cầu không commit
Báo cáo không coi file hiện tại là bằng chứng Git xác nhận trình tự prediction-first

## Phạm vi bằng chứng

Đã loại lượt thu RTA lúc 19:42:18 vì toàn bộ dữ liệu là byte NUL
Đã loại lượt thu Blinky tuned lúc 20:09:01 vì thiếu header,
nội dung còn lại giống bản hoàn chỉnh lúc 20:09:54

Giữ hai log baseline Blinky và Trace dù thiếu phần đầu
Chỉ dùng các dòng stack hợp lệ của chúng để đối chiếu baseline,
không dùng thay cho ba log hoàn chỉnh sau tinh chỉnh

Người học trực tiếp nạp firmware và thu UART
Đã có ảnh console Verified OK của lượt nạp static RTA baseline
Không có file console OpenOCD được lưu để đối chiếu từng lượt nạp cuối

Log cuối chứng minh các firmware đã chạy đến điểm báo cáo
Các lượt thử hữu hạn chưa thay thế cho kiểm thử chạy kéo dài
Chưa có phép đo logic analyzer riêng của W1E4 trong bộ bằng chứng này

## Trade-off

Cấp phát tĩnh dành trước bộ nhớ cho task, queue và mutex
Firmware cuối không cần tạo hoặc giải phóng các vùng heap cho những đối tượng này
Vì vậy không có chuỗi alloc/free gây phân mảnh heap trong quá trình vận hành

Đổi lại, kích thước và số lượng đối tượng phải được dự trù
RAM dành cho một stack vẫn bị giữ ngay cả khi task chưa dùng hết

Trong bản RTA, section RAM giảm từ 9032 xuống 6472 byte sau tinh chỉnh
Task còn ít stack nhất trong cả ba bản là TraceB với 304 byte
Các số này cho thấy lợi ích của đo high-water mark trước khi chọn lại kích thước

Nếu thêm chức năng, thay call path hoặc đổi cấu hình kernel,
cần đo lại high-water mark và cập nhật bảng RAM
Không dùng mức stack hiện tại như bảo đảm cho mọi phiên bản sau
