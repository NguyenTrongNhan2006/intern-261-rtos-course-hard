W1.4 – PREDICTION

MSSV 2452882

Ngày làm 16/10/2026, 
## Dự đoán phân mảnh

**Trả lời trực tiếp theo mẫu**

Có phân mảnh tạm thời khi free các khối 2, 1, 0 trong lúc khối 3 vẫn còn được cấp phát

Khối 3 ngăn hai vùng trống liền kề gộp lại, khi free khối 3 ở cuối vòng thì heap_4 gộp hai phía thành một vùng liên tục

Số byte bị mất tồn dư do phân mảnh sau khi free đủ 6 khối dự kiến bằng 0 byte, với điều kiện mọi cấp phát thành công và không có cấp phát khác xen vào

Minimum-ever cuối cùng dự kiến bằng min(M0, F0 - 872), không dùng chênh lệch giữa F0 và minimum-ever làm số byte bị mất do phân mảnh

F0 là tổng heap trống sau khởi tạo và trước chuỗi thử, M0 là minimum-ever tại cùng thời điểm

**Tham số đã đối chiếu với tools/gen_params.py**

`W1E4_NUM_BLOCKS = 6`

Mỗi vòng cấp phát đủ 6 khối theo chỉ số từ 0 đến 5

`W1E4_BLOCK_SIZE_BYTES = {320, 128, 56, 24, 200, 96}`

Đây là số byte dữ liệu yêu cầu cho từng khối, chưa bao gồm header quản lý của heap_4

`W1E4_FREE_ORDER = {5, 4, 2, 1, 0, 3}`

Giải phóng khối theo chỉ số này, không sắp xếp lại theo kích thước

`W1E4_REPEAT_CYCLES = 6`

Mỗi vòng có 6 lần cấp phát và 6 lần giải phóng, tổng cộng 72 dòng log sau thao tác

`W1E4_RAM_BUDGET_TOLERANCE_BYTES = 123`

Độ lệch tuyệt đối giữa tổng RAM tính tay và tổng RAM trong map phải không vượt quá 123 byte khi so cùng phạm vi

**Điều kiện để dự đoán có hiệu lực**

Dùng port GCC ARM_CM3, pointer và size_t dài 4 byte, portBYTE_ALIGNMENT bằng 8

BlockLink_t gồm một pointer và một size_t, nên xHeapStructSize bằng 8 byte

Thí nghiệm bắt đầu từ một vùng heap trống liên tục đủ lớn, cấp phát theo thứ tự 0 đến 5, mọi lần cấp phát thành công và không có task hoặc driver khác cấp phát xen vào

Phần còn lại sau mỗi lần cắt khối phải lớn hơn heapMINIMUM_BLOCK_SIZE bằng 16 byte để không bị nhận thêm phần đuôi không thể tách

Gọi F0 là số byte trống sau khi heap đã được khởi tạo và trước chuỗi thử nghiệm, gọi M0 là minimum-ever tại cùng thời điểm

Không lấy số trả về trước lần cấp phát đầu tiên làm F0 vì heap_4 trong repo khởi tạo lười, hai bộ đếm ban đầu bằng 0

Nếu cần khởi tạo trước thì phải mô tả lần cấp phát và giải phóng chuẩn bị, ghi riêng ngoài 72 bước chính và tính ảnh hưởng của nó tới M0

**Tính số byte heap bị trừ**

Công thức mỗi khối là align_up(payload + 8, 8)

| Khối | Payload byte | Header byte | Padding byte | Heap bị trừ byte |
| --- | ---: | ---: | ---: | ---: |
| 0 | 320 | 8 | 0 | 328 |
| 1 | 128 | 8 | 0 | 136 |
| 2 | 56 | 8 | 0 | 64 |
| 3 | 24 | 8 | 0 | 32 |
| 4 | 200 | 8 | 0 | 208 |
| 5 | 96 | 8 | 0 | 104 |
| Tổng | 824 | 48 | 0 | 872 |

Không gọi 48 byte header là dữ liệu bị mất do phân mảnh, chúng cũng được trả lại khi free khối

**Dự đoán log một vòng**

Bảng dưới giả sử M0 không nhỏ hơn F0 - 872

Ở vòng đầu, minimum-ever sau cấp phát thứ k là min(M0, F0 - tổng byte đã cấp phát)

Ở các vòng sau, minimum-ever giữ nguyên F0 - 872 vì vòng đầu đã chạm mức này

| Bước trong vòng | Thao tác | free_bytes_now dự kiến | free_bytes_min_ever sau thao tác |
| ---: | --- | --- | --- |
| 1 | alloc khối 0 | F0 - 328 | min(M0, F0 - 328) ở vòng đầu |
| 2 | alloc khối 1 | F0 - 464 | min(M0, F0 - 464) ở vòng đầu |
| 3 | alloc khối 2 | F0 - 528 | min(M0, F0 - 528) ở vòng đầu |
| 4 | alloc khối 3 | F0 - 560 | min(M0, F0 - 560) ở vòng đầu |
| 5 | alloc khối 4 | F0 - 768 | min(M0, F0 - 768) ở vòng đầu |
| 6 | alloc khối 5 | F0 - 872 | F0 - 872 |
| 7 | free khối 5 | F0 - 768 | F0 - 872 |
| 8 | free khối 4 | F0 - 560 | F0 - 872 |
| 9 | free khối 2 | F0 - 496 | F0 - 872 |
| 10 | free khối 1 | F0 - 360 | F0 - 872 |
| 11 | free khối 0 | F0 - 32 | F0 - 872 |
| 12 | free khối 3 | F0 | F0 - 872 |

Nếu M0 đã thấp hơn F0 - 872 do hoạt động trước đó, minimum-ever cuối cùng giữ M0

Công thức tổng quát sau cả 6 vòng là free_now = F0 và min_ever = min(M0, F0 - 872)

**Vì sao có phân mảnh tạm thời**

Sau khi cấp phát đủ 6 khối, thứ tự địa chỉ tăng dần là khối 0, 1, 2, 3, 4, 5 và vùng trống cuối heap

Free khối 5 rồi khối 4 nối chúng với vùng trống cuối heap

Free khối 2 tạo một vùng trống 64 byte nằm trước khối 3 còn sống, tách khỏi vùng trống phía sau

Free khối 1 rồi khối 0 làm vùng trống phía trước tăng từ 64 lên 200 rồi 528 byte

Ngay trước lần free cuối, khối 3 chiếm 32 byte và ngăn hai vùng trống gộp với nhau

Free khối 3 nối hai phía thành một vùng liên tục, nên không có phân mảnh tồn dư của chuỗi thử sau khi giải phóng đủ

Dự đoán số byte bị mất vĩnh viễn do phân mảnh của chuỗi thử là 0 byte trong các điều kiện đã nêu

`xPortGetMinimumEverFreeHeapSize()` không tăng trở lại khi free vì đây là mốc thấp nhất trong lịch sử

Không suy ra có rò rỉ 872 byte chỉ vì minimum-ever thấp hơn F0, và cũng không dùng riêng hai bộ đếm tổng để chứng minh mức phân mảnh

Khi đo có thể bổ sung vPortGetHeapStats để quan sát số vùng trống và kích thước vùng trống lớn nhất, nhưng vẫn phải giữ đủ hai trường log đề yêu cầu

PARAMS.md do generator tạo có câu gọi heap_4 là best-fit, mã nguồn heap_4 thực tế duyệt danh sách theo địa chỉ và chọn khối đầu tiên đủ lớn, tức first-fit

**Ví dụ có điều kiện cho heap 8192 byte**

Nếu ucHeap được căn 8 byte, chưa có object nào chiếm heap và chỉ mất 8 byte end marker thì F0 = 8184

Khi đó mức thấp nhất của chuỗi là 7312 byte và cuối mỗi vòng free_now trở về 8184 byte

Đây là ví dụ tính toán, không phải số đo hoặc cam kết F0 của firmware sẽ bằng 8184

## Dự đoán ngân sách RAM tĩnh

**Trả lời trực tiếp theo mẫu**

Tổng object RAM dự kiến bằng 15888 byte, đã bao gồm task stacks, TCB và buffer tĩnh, không cộng lại các thành phần này sau khi lấy tổng section

Phần RAM còn lại sau object là 20480 - 15888 = 4592 byte, trong đó dành riêng 1024 byte cho MSP theo linker hiện tại

Tổng ngân sách gồm MSP bằng 16912 byte và phần chưa sử dụng dự kiến còn 3568 byte

Các con số này chỉ áp dụng cho phương án 4 RTA task, Idle, Timer daemon, một queue và một mutex được liệt kê bên dưới, cần tính lại nếu thay đổi thiết kế

**Phương án ban đầu để tiếp tục coding**

Lấy workload W1.3 V2 hiện có làm cấu hình W1.4 dùng để đối chiếu RAM, giữ 4 RTA task với 256 word mỗi task

Dùng Idle stack 128 word, Timer daemon stack 256 word, bật timer service để thực hiện và kiểm tra cả hai callback cấp bộ nhớ

Thêm một queue tĩnh chứa 4 phần tử uint32_t và một mutex tĩnh để thực hành các API bắt buộc của syllabus, kiểm tra chức năng trước workload đo

Không tạo thêm software timer ở phương án này, chỉ có timer daemon và command queue nội bộ

W1.1 và W1.2 đang là các executable riêng, cần chuyển từng cấu hình ứng dụng được tái sử dụng sang static và kiểm tra riêng, không cộng RAM của ba executable như thể chúng chạy đồng thời

Bảng sau chỉ dự đoán cấu hình W1.4 dùng workload W1.3, chưa chứng minh việc chuyển đổi các cấu hình khác đã hoàn tất

Giữ configMAX_PRIORITIES = 5, configMAX_TASK_NAME_LEN = 16, configUSE_TRACE_FACILITY = 1, configUSE_APPLICATION_TASK_TAG = 1 và INCLUDE_vTaskSuspend = 1

Dự kiến configSUPPORT_STATIC_ALLOCATION = 1, configSUPPORT_DYNAMIC_ALLOCATION = 0, configUSE_MUTEXES = 1, configUSE_TIMERS = 1 và configCHECK_FOR_STACK_OVERFLOW = 2

Timer priority = 1, timer command queue length = 4, INCLUDE_xTimerPendFunctionCall = 0

Không dùng newlib reentrancy, queue registry, runtime stats hoặc tickless trong dự toán này

Kiểm tra ABI bằng ARM GCC 14.2.1 qua stdin, không tạo object hoặc firmware, cho sizeof(StackType_t) = 4, sizeof(StaticTask_t) = 96 và sizeof(StaticQueue_t) = 80 với cấu hình dự kiến trên

Những kích thước này phụ thuộc cấu hình kernel, không áp dụng máy móc khi đổi config

| Thành phần | Cách tính tay | RAM dự kiến byte |
| --- | --- | ---: |
| Stack 4 RTA task | 4 × 256 × 4 | 4096 |
| Idle stack | 128 × 4 | 512 |
| Timer daemon stack | 256 × 4 | 1024 |
| TCB của 6 task | 6 × 96 | 576 |
| Control block queue ứng dụng | 1 × 80 | 80 |
| Storage queue ứng dụng | 4 × sizeof(uint32_t) | 16 |
| Control block mutex | 1 × 80, không cần payload storage | 80 |
| Hai handle queue và mutex | 2 × 4 | 8 |
| Bảng high-water mark dự kiến | 6 × sizeof(UBaseType_t) | 24 |
| Tracer W1.2 | 256 × 8 + 4 × 4 | 2064 |
| records của rta_measure | 210 × 32 | 6720 |
| Các biến khác của rta_measure | calibration 64 + IRQ 40 + active_irq 4 + 5 mảng 16 + 6 scalar 4 | 212 |
| handles và indices của rta_app | 2 × 4 × 4 | 32 |
| SystemCoreClock | 1 × uint32_t | 4 |
| Scheduler globals dự kiến | 9 List_t × 20 + 15 object 4 byte, kể cả uxTopUsedPriority nếu còn được link | 240 |
| Timer globals và command queue | 2 List_t × 20 + 5 object 4 + StaticQueue_t 80 + 4 message × 12 | 188 |
| Port ARM_CM3 globals | 4 + 1 + 4, dự trù 3 byte alignment | 12 |
| Tổng object RAM dự kiến | Đã gồm task stacks và buffer tĩnh | 15888 |
| MSP dành riêng theo linker hiện tại | _Min_Stack_Size = 0x400 | 1024 |
| Tổng RAM dự kiến gồm MSP | 15888 + 1024 | 16912 |
| Còn dư so với 20 KiB | 20480 - 16912 | 3568 |

15888 byte là dự toán ban đầu theo object và cấu hình đã nêu, chưa phải kết quả link, việc linker loại symbol không dùng hoặc chèn alignment sẽ tạo chênh lệch phải giải thích

Task stack, TCB và các mảng đã nằm trong .bss hoặc .data, không cộng chúng lần thứ hai sau khi đã lấy tổng section từ map

Mảng heap 8192 byte cùng mọi symbol của heap_4 phải vắng mặt ở firmware static cuối

Không giữ buffer log heap trong firmware static cuối, ở giai đoạn thí nghiệm sẽ gửi UART sau từng bước để không cần mảng 72 bản ghi

Các bảng params.h là static const, linker hiện tại đưa .rodata vào FLASH, không tính chúng vào RAM nếu bản link giữ đúng cách đặt section này

MSP được giữ lại bằng __StackLimit và ASSERT trong linker hiện tại, không có output section .stack riêng nên cần cộng đúng 1024 byte khi tính phần RAM còn dư

Công thức đối chiếu là abs(RAM_sections_từ_map - RAM_objects_tính_tay) <= 123 byte

Nếu cộng MSP thì phải cộng cùng 1024 byte ở cả hai vế, đồng thời ghi riêng .data, .bss, .noinit và mọi section khác có VMA trong RAM

**Dự đoán stack và điều kiện cập nhật**

uxTaskGetStackHighWaterMark trả về số word stack chưa từng dùng, đổi sang byte bằng cách nhân với 4 trên port ARM_CM3

Các kích thước stack phía trên là lựa chọn ban đầu, chưa được chứng minh an toàn bằng phép đo

Dự kiến kiểm tra Idle hook khi dump UART, đường assert, timer service và toàn bộ workload trước khi giảm stack

Chưa dùng số đo của W1.3 cũ để kết luận stack hoặc timing của firmware static mới

Nếu thiết kế cuối thêm task, thay buffer, đổi cấu hình kernel hoặc đổi stack thì cần cập nhật dự toán trước lượt chạy tương ứng và giữ rõ lịch sử thay đổi

## Điều kiện thực hiện và xác minh

Giai đoạn thí nghiệm heap giữ configSUPPORT_DYNAMIC_ALLOCATION = 1 theo ngoại lệ nêu trực tiếp trong README W1.4

check_api.sh hiện quét văn bản và sẽ báo DYNAMIC nếu còn lời gọi cấp phát động trong source W1.4, kể cả đặt chúng trong nhánh #if không được biên dịch

Không sửa checker, không đổi tên lời gọi hoặc chuyển code sang nơi khác để né kiểm tra

Sau khi lưu đủ log của giai đoạn heap, chuyển source bản cuối sang static, loại heap_4 khỏi target và kiểm tra cả source lẫn symbol của ELF

configSUPPORT_DYNAMIC_ALLOCATION = 0 là cấu hình compile-time, cần build firmware mới, không phải cờ có thể đổi trực tiếp khi chương trình đang chạy

Tái dùng interface heap_experiment.h đã có sẵn, không tạo header trùng tên hoặc thư mục deliverables mới

[CẦN BỔ SUNG] Firmware W1.4, build CMake + Ninja với -Wall -Wextra -Werror, map thật và kiểm tra 0 cấp phát động

[CẦN BỔ SUNG] 72 dòng log heap trên board, high-water mark từng task, đối chiếu ngân sách RAM và REPORT

Những bảng trong file này là dự đoán trước phép đo, không phải log STM32 hoặc kết quả kiểm chứng phần cứng

## Điều chỉnh thiết kế trước lượt đo stack tinh chỉnh

Giữ phần dự đoán ban đầu phía trên làm lịch sử

Firmware hiện tại không giữ mảng JOB2 của phương án ban đầu,
nên dự toán RAM 15888 byte không còn mô tả thiết kế đang chạy

Phương án tính tay 9028 byte đã được cung cấp trong hướng dẫn
trước lượt đo baseline nhưng chưa được cập nhật vào file này

Map baseline RTA thực tế dùng 9032 byte cho các section RAM

Các lượt baseline quan sát được mức dùng stack lớn nhất
52 word ở TraceB, 48 word ở Idle và 32 word ở Timer

Trước lượt đo mới, dự kiến giảm stack ứng dụng từ 256 xuống
128 word và Timer từ 256 xuống 128 word

Idle giữ nguyên 128 word

Mục tiêu kiểm tra là còn ít nhất 32 word trên từng task
sau các đường chạy đang được thử

| Target | RAM section baseline | Giảm dự kiến | RAM section dự kiến mới |
| --- | ---: | ---: | ---: |
| w1e4 | 9032 | 2560 | 6472 |
| w1e4_blinky | 4632 | 1536 | 3096 |
| w1e4_trace | 6728 | 1536 | 5192 |

Ngân sách tính tay mới cho object của target RTA
là 9028 - 2560 = 6468 byte

MSP vẫn dành riêng 1024 byte

Map và high-water mark sau tinh chỉnh [CẦN BỔ SUNG]

Các con số dự kiến mới chưa phải kết quả đo sau tinh chỉnh


[FreeRTOS memory management](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/09-Memory-management/01-Memory-management)

[GNU ld về link map](https://sourceware.org/binutils/docs/ld/Options.html)

Mã nguồn đã đọc trực tiếp trong repo gồm portable/MemMang/heap_4.c, portable/GCC/ARM_CM3/portmacro.h, tasks.c, timers.c cùng các module W1.3 V2 và tracer W1.2
