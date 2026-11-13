# W3.4 – PREDICTION: phân tích kernel trước khi chạy hộp đen

## 1. Thông số bài tập và mục tiêu điều tra – MSSV 2452882

| Thông số | Giá trị / phép tính |
|---|---|
| Bộ sinh | `python -B tools/gen_params.py 2452882 --only w3e4` |
| `W3E4_MIN_GDB_EVIDENCE_COMMANDS` | `6`: ít nhất 6 lệnh khác nhau có output và giải thích |
| `W3E4_RAM_SCAN_START` | `0x20003300` |
| `W3E4_RAM_SCAN_LEN` | `0x800 = 2048` byte |
| Địa chỉ cuối, tính cả hai đầu | `0x20003300 + 0x800 - 1 = 0x20003AFF` |
| Số word 32-bit | `0x800 / 4 = 512` |
| Lệnh quét bắt buộc | `x/512xw 0x20003300` |
| Ba giả thuyết | Deadlock; Stack Overflow; sai priority của Interrupt Service Routine (ISR, trình phục vụ ngắt) |

- Trạng thái tại Chốt 1: chưa có `week3/ex4/w3e4.elf`; chưa đọc symbol hộp đen, chưa kết nối/chạy GDB với board, chưa có ảnh chụp RAM.
  + Giải thích: đây là dự đoán có điều kiện và kịch bản thu bằng chứng. Không điền địa chỉ TCB, số task/mutex, priority ISR hoặc nguyên nhân lỗi bằng dữ liệu của bài khác.
- Symbol cần tìm: `pxCurrentTCB`, `pxReadyTasksLists`, `pxDelayedTaskList`, `pxOverflowDelayedTaskList`, `xDelayedTaskList1`, `xDelayedTaskList2`, `xSuspendedTaskList`, `xPendingReadyList`, `uxCurrentNumberOfTasks`, `xTickCount`, buffer TCB/stack tĩnh, handle/buffer mutex và hàm ISR. Địa chỉ/kích thước: **chưa xác định vì chưa có ELF**; symbol tùy chọn có thể không tồn tại.
- Chưa có cơ sở xếp một lỗi là khả năng cao nhất. Nếu ELF có nhiều task và ít nhất hai mutex, ưu tiên dựng đồ thị chờ khóa; nếu có stack nhỏ hoặc đường gọi sâu, ưu tiên kiểm tra biên stack; nếu ISR gọi API kernel, ưu tiên đối chiếu ngưỡng priority. Số symbol chỉ định hướng, không xác nhận lỗi.
- Dự kiến dùng khoảng `18..30` lệnh chẩn đoán khác nhau, có thể tăng khi phải duyệt nhiều task. Lệnh kết nối, thiết lập biến hoặc bật log không thay cho bằng chứng; tối thiểu 6 lệnh phải cho dữ liệu thực dùng trong lập luận.
- Điều kiện kết luận: có bằng chứng định lượng cho một giả thuyết và bằng chứng phản bác riêng cho từng giả thuyết còn lại. Một lần halt ở Idle, một word stack mất mẫu hoặc một IRQ có priority cao chưa đủ.
- Cập nhật Chốt 2: đã có `w3e4.elf`. Kết quả phân tích tĩnh và các chỗ dự đoán Chốt 1 phải sửa nằm ở mục 6. Các mục 1..5 giữ nguyên làm mốc so sánh.

## 2. Đối chiếu cấu trúc thật và hai bẫy trong `tcb_reference.h`

Nguồn trực tiếp: [`tasks.c`](../../third_party/FreeRTOS-Kernel/tasks.c) (`tskTaskControlBlock`), [`list.h`](../../third_party/FreeRTOS-Kernel/include/list.h) (`xLIST_ITEM`, `xMINI_LIST_ITEM`, `xLIST`), [`queue.c`](../../third_party/FreeRTOS-Kernel/queue.c) (`QueueDefinition`), [`FreeRTOS.h`](../../third_party/FreeRTOS-Kernel/include/FreeRTOS.h), [`portmacro.h`](../../third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/portmacro.h) và [`tcb_reference.h`](include/tcb_reference.h). `task.h` ghi phiên bản kernel `V11.1.0`.

### 2.1. Điều kiện áp dụng bảng byte-offset

| Điều kiện | Giá trị dùng để tính bảng |
|---|---|
| ABI / port | ARM 32-bit, GCC `ARM_CM3`, con trỏ / `UBaseType_t` / `StackType_t` / `TickType_t` đều 4 byte; căn hàng struct 4 byte |
| Hướng stack | `portSTACK_GROWTH = -1` |
| List | `configUSE_MINI_LIST_ITEM = 1`, `configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES = 0` |
| Trường trước tên task | `portUSING_MPU_WRAPPERS = 0`, `configNUMBER_OF_CORES = 1`, `configMAX_TASK_NAME_LEN = 16` |
| Trường tùy chọn TCB | `configUSE_TASK_PREEMPTION_DISABLE = 0`, `portCRITICAL_NESTING_IN_TCB = 0`, `configUSE_MUTEXES = 1` |
| Task notification | `configUSE_TASK_NOTIFICATIONS = 1`, `configTASK_NOTIFICATION_ARRAY_ENTRIES = 1` |
| Các phần mở rộng sau mutex | Task tag, thread-local storage, runtime statistics, C-runtime TLS và POSIX errno đều tắt; `INCLUDE_xTaskAbortDelay = 0` |
| Cấp phát của bảng chính | `configSUPPORT_STATIC_ALLOCATION = 1`, `configSUPPORT_DYNAMIC_ALLOCATION = 0` |

Các điều kiện trên là **giả định để xây bảng ứng viên**, chưa phải cấu hình ELF Mentor. Vùng `0..67` giống nhau giữa A/B0/B1 dưới các điều kiện này; **không bất biến với mọi cấu hình FreeRTOS**. MPU, nhiều core, integrity bytes, tick 64-bit hoặc độ dài tên khác có thể làm lệch các trường trước byte 68. Nếu cho phép cả cấp phát tĩnh và động, `ucStaticallyAllocated` xuất hiện sau notification; trong nhóm này nó có thể chiếm padding mà không đổi `sizeof(TCB_t)`.

### 2.2. Bẫy 1: `xListEnd` là mini item, stride của `List_t` là 20 byte

| Cấu trúc / trường | Offset byte | Kích thước byte |
|---|---:|---:|
| `ListItem_t.xItemValue` | 0 | 4 |
| `ListItem_t.pxNext` | 4 | 4 |
| `ListItem_t.pxPrevious` | 8 | 4 |
| `ListItem_t.pvOwner` | 12 | 4 |
| `ListItem_t.pxContainer` | 16 | 4 |
| Tổng `ListItem_t` | — | 20 |
| `MiniListItem_t`: `xItemValue`, `pxNext`, `pxPrevious` | 0, 4, 8 | 12 |
| `List_t.uxNumberOfItems` | 0 | 4 |
| `List_t.pxIndex` | 4 | 4 |
| `List_t.xListEnd.xItemValue` | 8 | 4 |
| `List_t.xListEnd.pxNext` | 12 | 4 |
| `List_t.xListEnd.pxPrevious` | 16 | 4 |
| Tổng `List_t` thật | — | `4 + 4 + 12 = 20` |
| Tổng `List_t_ref` trong header tham khảo | — | `4 + 4 + 20 = 28` |

- `addr_ready(prio) = addr_pxReadyTasksLists + 20 * prio`; cách tính sai là `base + 28 * prio`, lệch `8 * prio` byte. Ví dụ `prio = 4` lệch `32` byte.
- Sentinel (phần tử đánh dấu cuối) ở `list_base + 8`, `xItemValue = 0xFFFFFFFF` với tick 32-bit. List rỗng có `uxNumberOfItems = 0` và hai liên kết sentinel trỏ về chính sentinel; không đọc `pvOwner`/`pxContainer` ở sentinel vì mini item không có hai trường này.
- `pxIndex` là con trỏ phục vụ luân phiên chọn task, không nhất thiết là đầu list. Duyệt từ `xListEnd.pxNext`, dừng khi quay về sentinel và đối chiếu số node với `uxNumberOfItems`.
- Tên thật trong kernel là `pxContainer`; header tham khảo dùng `pvContainer`. Khi nói `xEventListItem.pvContainer` theo đề, hiểu đó là `xEventListItem.pxContainer` tại TCB offset `40` trong bảng này.
- Nếu ELF bật `configUSE_MINI_LIST_ITEM = 0`, `MiniListItem_t` là alias của `ListItem_t`, khi đó list 28 byte có thể đúng. Phải xác nhận stride ELF trước khi dùng `20` hoặc chia kích thước mảng cho `20`.

### 2.3. Bẫy 2: `pxEndOfStack` có điều kiện; phần đuôi TCB không cố định

Trong `tasks.c`, trường này nằm trong điều kiện:

```c
#if ((portSTACK_GROWTH > 0) || (configRECORD_STACK_HIGH_ADDRESS == 1))
    StackType_t *pxEndOfStack;
#endif
```

Trên Cortex-M3, `portSTACK_GROWTH = -1`, nên chỉ có `pxEndOfStack` khi `configRECORD_STACK_HIGH_ADDRESS == 1`. Header tham khảo luôn khai báo nó nên không thể dùng trực tiếp cho biến thể A.

| Vùng chung TCB (A/B0/B1) | Offset byte | Độ rộng / khoảng byte |
|---|---:|---|
| `pxTopOfStack` | 0 | 4; `0..3` |
| `xStateListItem` | 4 | 20; `4..23` |
| `xStateListItem.pvOwner` / `.pxContainer` | 16 / 20 | Mỗi trường 4 |
| `xEventListItem` | 24 | 20; `24..43` |
| `xEventListItem.pvOwner` / `.pxContainer` | 36 / 40 | Mỗi trường 4 |
| `uxPriority` | 44 | 4; `44..47` |
| `pxStack` | 48 | 4; `48..51` |
| `pcTaskName[16]` | 52 | 16; `52..67` |

| Trường / kích thước | A: RECORD=0, TRACE=0 | B0: RECORD=1, TRACE=0 | B1: RECORD=1, TRACE=1 |
|---|---:|---:|---:|
| `pxEndOfStack` | Không có | 68 | 68 |
| `uxTCBNumber` | Không có | Không có | 72 |
| `uxTaskNumber` | Không có | Không có | 76 |
| `uxBasePriority` | 68 | 72 | 80 |
| `uxMutexesHeld` | 72 | 76 | 84 |
| `ulNotifiedValue[0]` | 76 | 80 | 88 |
| `ucNotifyState[0]` | 80 (1 byte) | 84 (1 byte) | 92 (1 byte) |
| Padding cuối, với bảng cấu hình chính | `81..83` | `85..87` | `93..95` |
| `sizeof(TCB_t)` | `84 = 0x54` | `88 = 0x58` | `96 = 0x60` |

`RECORD` và `TRACE` lần lượt là `configRECORD_STACK_HIGH_ADDRESS` và `configUSE_TRACE_FACILITY`.

- Với A, đọc `TCB + 68` như con trỏ stack thực chất là đọc `uxBasePriority`; các trường sau trong header cũng lệch 4 byte. Với B1, hai số trace thêm 8 byte trước `uxBasePriority`, nên header vẫn sai dù có `pxEndOfStack`.
- Cấu hình `RECORD=0, TRACE=1` cũng hợp lệ: hai số trace ở `68/72`, base priority `76`, mutex count `80`, notification value/state `84/88`, tổng `92 = 0x5C` byte. Kích thước không phải mã nhận dạng cấu hình duy nhất.
- `uxPriority` là priority hiện tại; `uxBasePriority` là priority nền. Priority inheritance có thể làm `uxPriority > uxBasePriority`; `uxMutexesHeld > 0` chỉ là số mutex đang giữ, không chỉ ra địa chỉ từng mutex.
- `pxTopOfStack` lưu con trỏ ngữ cảnh lần lưu gần nhất. Với task đang chạy, nó không nhất thiết bằng Process Stack Pointer (PSP) hiện tại; Main Stack Pointer (MSP) phục vụ handler. Không so sánh `$sp` trong ISR với stack task để kết luận tràn.
- `xStateListItem` biểu thị Ready/Delayed/Suspended. `xEventListItem` thường dùng cho danh sách chờ queue/semaphore; chờ direct notification được theo dõi bằng trạng thái notification, không có một “notify wait list” chung. Event container bằng `0` không đồng nghĩa task Ready.

### 2.4. Byte-offset `Queue_t`: mutex holder ở +8, không phải +4

`Queue_t` bắt đầu bằng **hai con trỏ** `pcHead`, `pcWriteTo`, rồi mới đến union `u`. Yêu cầu “`u.xSemaphore.xMutexHolder` ở offset 4” không khớp `queue.c` của repo; offset đúng là **8**.

| Trường | Offset byte | Độ rộng byte | Cách hiểu |
|---|---:|---:|---|
| `pcHead` (`uxQueueType`) | 0 | 4 | `NULL` là dấu nhận diện mutex trong cấu trúc hợp lệ |
| `pcWriteTo` | 4 | 4 | Con trỏ ghi queue; không phải mutex holder |
| `u.xQueue.pcTail` / `u.xSemaphore.xMutexHolder` | 8 | 4 | Với mutex: con trỏ TCB chủ khóa, `NULL` nếu chưa được giữ |
| `u.xQueue.pcReadFrom` / `u.xSemaphore.uxRecursiveCallCount` | 12 | 4 | Bộ đếm lấy khóa lặp cho recursive mutex |
| `xTasksWaitingToSend` | 16 | 20 | List task chờ gửi |
| `xTasksWaitingToReceive` | 36 | 20 | List task chờ nhận / lấy semaphore / lấy mutex |
| `uxMessagesWaiting` | 56 | 4 | Với mutex hợp lệ: thường `0` khi đã bị giữ, `1` khi khả dụng |
| `uxLength` | 60 | 4 | Mutex có chiều dài `1` |
| `uxItemSize` | 64 | 4 | Semaphore/mutex có item size `0` |
| `cRxLock` / `cTxLock` | 68 / 69 | Mỗi trường 1 | Bộ đếm khóa nội bộ queue, không phải chủ mutex |

Với cấu hình bảng chính, queue sets và trace tắt, `sizeof(Queue_t) = 72` sau padding. Trường cấp phát hỗn hợp/queue sets/trace có thể thêm phần đuôi; thay đổi layout list làm lệch cả hai wait list và các trường sau chúng. Chỉ đọc union như mutex sau khi kiểm tra kiểu, các giá trị nhất quán và membership của wait list; `uxItemSize == 0` riêng lẻ chưa phân biệt mutex với semaphore.

## 3. Ba cách xác định layout của chính `w3e4.elf`

### 3.1. DWARF, nếu có thông tin kiểu

```powershell
arm-none-eabi-gdb -nx -batch -ex "ptype /o TCB_t" -ex "ptype /o List_t" -ex "ptype /o Queue_t" week3/ex4/w3e4.elf
arm-none-eabi-readelf -S week3/ex4/w3e4.elf
```

Debugging With Attributed Record Formats (DWARF) có thể còn trong ELF dù không được cấp source. Nếu typedef không có, thử `ptype /o struct tskTaskControlBlock` hoặc `ptype /o struct QueueDefinition`. Không tìm thấy kiểu là lý do chuyển sang hai cách sau, không phải bằng chứng file hỏng. Các lệnh batch này chỉ phân tích ELF, không kết nối board.

### 3.2. Kích thước symbol trong `.symtab`, không cần DWARF

```powershell
arm-none-eabi-nm -S -n week3/ex4/w3e4.elf
arm-none-eabi-readelf -sW week3/ex4/w3e4.elf
arm-none-eabi-nm -S -n week3/ex4/w3e4.elf | Select-String 'pxCurrentTCB|pxReadyTasksLists|DelayedTaskList|SuspendedTaskList|PendingReadyList|TCB|Stack|Mutex|Semaphore|IRQHandler'
```

- `nm -S` in địa chỉ và size dạng hex; `readelf -sW` có cột `Size` thường ở dạng thập phân. Tìm symbol `OBJECT` là buffer TCB tĩnh thực (`StaticTask_t`), không lấy size `4` của một `TaskHandle_t` làm kích thước TCB.
- Các size `0x54`, `0x58`, `0x60` gợi ý A/B0/B1 trong nhóm giả định; `0x5C` còn có thể là RECORD=0/TRACE=1. Buffer gộp, mảng TCB, padding hoặc cấu hình khác có thể tạo cùng size. Đối chiếu thêm disassembly; không suy cấu hình chỉ từ một con số.
- Sau khi xác nhận list stride `20`: `configMAX_PRIORITIES = st_size(pxReadyTasksLists) / 20`, kiểm tra số dư bằng `0`. Ví dụ **có điều kiện**: size `0x64 = 100` cho `5` mức; size `0x3C = 60` cho `3` mức. Chưa có ELF nên chưa chọn số nào.
- Chỉ strip DWARF nhưng giữ `.symtab` thì cách này còn dùng được. Nếu bị xóa cả symbol, hoặc symbol size bằng `0`, không áp dụng phép chia/tra tên như trên; phải truy địa chỉ qua mã máy. Cấp phát động cũng có thể không có symbol riêng cho từng TCB.

### 3.3. Disassembly: lần theo con trỏ nền rồi mới diễn giải `#imm`

```powershell
arm-none-eabi-objdump -d week3/ex4/w3e4.elf
arm-none-eabi-objdump -d --disassemble=prvInitialiseNewTask week3/ex4/w3e4.elf
arm-none-eabi-objdump -d --disassemble=pvTaskIncrementMutexHeldCount week3/ex4/w3e4.elf
arm-none-eabi-objdump -d --disassemble=xTaskPriorityInherit week3/ex4/w3e4.elf
arm-none-eabi-objdump -d --disassemble=xTaskGenericNotify week3/ex4/w3e4.elf
```

Tên đúng trong V11.1.0 là **`xTaskPriorityInherit`**, không phải `vTaskPriorityInherit` như gợi ý trong yêu cầu. Gợi ý disassembly trong header không thay thế việc xác nhận hàm thực sự tồn tại.

| Hàm | Mẫu cần lần theo | A / B0 / B1 |
|---|---|---|
| `prvInitialiseNewTask` | Con trỏ `pxNewTCB`, ghi priority, tên, stack; nhánh ghi địa chỉ stack cao | Priority `+44`, stack thấp `+48`, tên `+52`; high address chỉ có ở B, `+68` |
| `pvTaskIncrementMutexHeldCount` | Nạp giá trị `pxCurrentTCB`, đọc–tăng–ghi bộ đếm | `uxMutexesHeld` tại `+72 / +76 / +84` |
| `xTaskPriorityInherit` | Con trỏ TCB holder; đối chiếu priority và base priority | Current priority `+44`; base `+68 / +72 / +80` |
| `xTaskGenericNotify` | TCB đích và index notification, lệnh word/byte | Value `+76 / +80 / +88`; state `+80 / +84 / +92` |

`ldr/str ..., [rY, #imm]` chỉ chứng minh offset nếu đã xác nhận `rY` là đầu TCB. Compiler có thể cộng trước vào con trỏ, dùng `ldrb/strb`, `ldm/stm`, inline hoặc tạo hậu tố `.constprop`/`.isra`. Khi đó cộng tổng các độ dời, tìm call site hoặc bản clone; không kết luận trường vắng chỉ vì không thấy một lệnh đơn lẻ. Nếu các cách mâu thuẫn, dừng giải mã phần đuôi, kiểm tra lại cấu hình/kiểu/địa chỉ nền.

## 4. Chuỗi lệnh GDB: lấy địa chỉ từ symbol, đọc RAM không cần DWARF

### 4.1. Quy ước, nạp ELF và chụp trạng thái

- Các lệnh dưới đây là **kịch bản cho lượt có ELF/board**, chưa phải output đo. Chạy GDB từ gốc repo: `arm-none-eabi-gdb -nx week3/ex4/w3e4.elf`.
- Bộ lệnh giả định còn symbol và đã xác nhận các offset mục 2. Chỉ địa chỉ cửa sổ SRAM được giao và thanh ghi phần cứng là cố định; địa chỉ object kernel phải lấy từ ELF hoặc từ liên kết RAM.
- Không dùng `TCB_t_ref *`, `TCB_t *` hoặc mặc định typedef `uint32_t` tồn tại. Khi strip DWARF, `(uint32_t *)&pxCurrentTCB` cũng có thể lỗi vì mất typedef. Dùng kiểu C cơ bản `unsigned int` (kiểm tra 4 byte), `unsigned char` và `char *`, theo cơ chế cast của [GDB – Program Variables](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Variables.html).
- `&pxCurrentTCB` là địa chỉ biến chứa con trỏ; `*(unsigned int *)&pxCurrentTCB` mới là địa chỉ TCB. Với mảng `pxReadyTasksLists`, lấy `&pxReadyTasksLists` rồi cộng byte; không dereference word đầu như một handle.
- Nếu board đã chạy đúng ELF và đang tái hiện lỗi, attach rồi halt để giữ trạng thái. Khi cần nạp lần đầu, thực hiện riêng `monitor reset halt`, `load`, `monitor reset halt`, rồi cho chạy theo kịch bản tái hiện được giao trước khi chụp. Không `load`/reset đè lên trạng thái đang điều tra.

```gdb
set pagination off
set language c
set logging file w3e4-gdb.log
set logging overwrite off
set logging enabled on
target extended-remote :3333
monitor halt
info reg pc lr sp xpsr basepri
info reg msp psp control primask faultmask
x/8i $pc
bt
p sizeof(unsigned int)
p sizeof(void *)
x/512xw 0x20003300
```

Hai `sizeof` phải bằng `4`. Nếu GDB/OpenOCD không cung cấp một tên thanh ghi, xem `info all-registers` và ghi rõ hạn chế. `bt` có thể thiếu frame khi không có unwind/debug hoặc stack đã hỏng; không có backtrace đẹp chưa phải bằng chứng stack overflow. Quét cửa sổ được giao trước khi đọc RAM qua symbol; không thấy TCB trong cửa sổ này không có nghĩa firmware không có task.

### 4.2. Task đang chạy và các trường chung

```gdb
info address pxCurrentTCB
p/x *(unsigned int *)&pxCurrentTCB
set $tcb = *(unsigned int *)&pxCurrentTCB
```

Trước khi đọc tiếp, xác nhận scheduler đã khởi động, `$tcb != 0`, địa chỉ căn 4 byte và nằm trong SRAM hợp lệ. Nếu `pxCurrentTCB == 0` lúc mới reset, chưa có snapshot task để phân loại ba lỗi.

```gdb
x/17xw $tcb
p/x *(unsigned int *)($tcb + 0)
p/u *(unsigned int *)($tcb + 44)
p/x *(unsigned int *)($tcb + 48)
x/16bx ($tcb + 52)
x/s ($tcb + 52)
p/x *(unsigned int *)($tcb + 20)
p/x *(unsigned int *)($tcb + 40)
```

- `x/17xw` đọc đúng vùng chung `68` byte. `x/s` cần tên có byte kết thúc `0` trong 16 byte; nếu không, dùng dump byte để tránh diễn giải quá cuối tên.
- Lệnh được gợi ý `x/24xw *(unsigned int *)&pxCurrentTCB` đọc `96` byte: chỉ dùng khi xác nhận TCB B1 và vùng nhớ hợp lệ; với A/B0 nó đọc quá object. Khi đã biết size, dùng `x/21xw $tcb` cho A, `x/22xw $tcb` cho B0, `x/24xw $tcb` cho B1.
- Chọn offset phần đuôi từ mục 2 sau khi xác nhận layout. Ví dụ **chỉ cho A**: `p/u *(unsigned int *)($tcb + 68)` đọc base priority; `p/u *(unsigned int *)($tcb + 72)` đọc mutex count; `p/x *(unsigned int *)($tcb + 76)` đọc notification value; `x/bx ($tcb + 80)` đọc notification state.

### 4.3. Ready, delayed, suspended và pending-ready lists

Từ cột size của `pxReadyTasksLists` trong `nm -S`, gán biến tiện ích `$ready_bytes` bằng số byte đã đọc. Ví dụ **chỉ nếu size thực là `00000064`**, nhập `set $ready_bytes = 0x64`; không dùng ví dụ này nếu ELF có size khác. Các biến tiện ích GDB nằm ở debugger, không ghi RAM board.

```gdb
set $ready = (unsigned int)&pxReadyTasksLists
p/u $ready_bytes % 20
set $nprio = $ready_bytes / 20
p/u $nprio
set $ntasks = *(unsigned int *)&uxCurrentNumberOfTasks
p/u $ntasks
set $prio = 0
while $prio < $nprio
  printf "priority=%u list=%#x\n", $prio, $ready + 20 * $prio
  x/5xw ($ready + 20 * $prio)
  set $prio = $prio + 1
end
x/5xw *(unsigned int *)&pxDelayedTaskList
x/5xw *(unsigned int *)&pxOverflowDelayedTaskList
```

Chỉ tiếp tục nếu số dư bằng `0` và `$nprio` hợp lý. `x/25xw (unsigned int *)&pxReadyTasksLists` là cách đọc 5 list đầu **chỉ khi mảng có ít nhất 5 mức**; không chứng minh mọi ELF có 5 priority, cũng chưa duyệt các node task.

Định nghĩa helper dưới đây cho list đã được xác nhận hợp lệ. Đối số thứ hai là offset node trong TCB: `4` cho state list, `24` cho event list. Gán biểu thức địa chỉ vào biến tiện ích trước khi gọi helper, vì GDB tách đối số user command theo khoảng trắng. Cửa sổ `0x20000000..0x20004FFF` là SRAM 20 KB của STM32F103C8 theo bài; nếu phần cứng khác, xác nhận và đổi giới hạn trước khi dùng.

```gdb
define w3e4_walk
  set $wl = (unsigned int)$arg0
  set $we = $wl + 8
  set $wc = *(unsigned int *)$wl
  set $wn = *(unsigned int *)($wl + 12)
  set $wi = 0
  set $wbad = 0
  while $wn != $we && $wi < $wc && $wi < $ntasks && $wi < 256 && $wbad == 0
    if $wn < 0x20000000 || $wn > 0x20005000 - 20 || ($wn & 3) != 0
      printf "invalid node=%#x\n", $wn
      set $wbad = 1
    else
      x/5xw $wn
      set $wo = *(unsigned int *)($wn + 12)
      set $wcontainer = *(unsigned int *)($wn + 16)
      printf "node=%#x owner=%#x container=%#x\n", $wn, $wo, $wcontainer
      if $wn != $wo + $arg1 || $wcontainer != $wl
        printf "owner/container mismatch: recheck layout or corruption\n"
        set $wbad = 1
      end
      set $wn = *(unsigned int *)($wn + 4)
      set $wi = $wi + 1
    end
  end
  printf "visited=%u declared=%u next=%#x sentinel=%#x\n", $wi, $wc, $wn, $we
  if $wi != $wc || $wn != $we
    printf "list not fully validated\n"
  end
end
set $prio = 0
while $prio < $nprio
  set $list_addr = $ready + 20 * $prio
  w3e4_walk $list_addr 4
  set $prio = $prio + 1
end
set $delayed = *(unsigned int *)&pxDelayedTaskList
set $overflow = *(unsigned int *)&pxOverflowDelayedTaskList
w3e4_walk $delayed 4
w3e4_walk $overflow 4
```

Với mỗi `owner` hợp lệ, đọc tên tại `owner + 52`, priority tại `owner + 44`, state/event container tại `owner + 20`/`owner + 40`. Đối chiếu thêm liên kết ngược: `node->pxNext->pxPrevious == node` và `node->pxPrevious->pxNext == node`; sentinel có cả hai trường nên vẫn kiểm tra được. Helper giới hạn theo `$ntasks` và tối đa 256 node để tránh vòng lặp dài nếu RAM hỏng; đây là kiểm tra nhất quán, không tự kết luận nguyên nhân nếu RAM hoặc layout sai.

Nếu các symbol tồn tại trong ELF:

```gdb
x/5xw (unsigned int)&xSuspendedTaskList
set $suspended = (unsigned int)&xSuspendedTaskList
w3e4_walk $suspended 4
x/5xw (unsigned int)&xPendingReadyList
set $pending = (unsigned int)&xPendingReadyList
w3e4_walk $pending 24
p/u *(unsigned int *)&xTickCount
```

- Task Running thường vẫn có state item trong ready list của nó trên port đơn core; không đếm nó thêm lần nữa. Ready list chỉ còn Idle phù hợp với nhiều task bị block nhưng chưa đủ chứng minh deadlock.
- Task chờ vô hạn (`portMAX_DELAY`, khi hỗ trợ suspend) có thể nằm ở `xSuspendedTaskList`; cần phân biệt với task bị suspend chủ động. `pxDelayedTaskList` và `pxOverflowDelayedTaskList` đổi vai khi tick wrap, nên đọc cả hai.
- `xPendingReadyList` chứa **event item**, không phải state item; task ở đây đang chờ xử lý chuyển sang Ready. Event container trỏ list này không thể được diễn giải thành địa chỉ mutex.
- State item trong delayed list dùng `xItemValue` làm wake tick. Thứ tự priority đảo thường nằm trong **event item** (`configMAX_PRIORITIES - uxPriority`, trừ khi giá trị được mượn cho mục đích khác), không phải dấu nhận dạng bắt buộc cho mọi ready-list item.

### 4.4. Truy task đang chờ sang mutex và holder

Chọn `$waiter` bằng địa chỉ `owner` vừa tìm được, không đoán địa chỉ. Với task đã xác nhận đang chờ **nhận/lấy mutex**, event container có thể là `&mutex->xTasksWaitingToReceive`, nên ứng viên queue là `event_container - 36`:

```gdb
set $ev = *(unsigned int *)($waiter + 40)
p/x $ev
```

Chỉ chạy phần tiếp nếu `$ev != 0`, không phải pending-ready list hoặc event list của loại object khác, và ngữ cảnh chờ đã được xác nhận từ symbol/disassembly/membership:

```gdb
set $q = $ev - 36
x/18xw $q
p/x *(unsigned int *)($q + 0)
p/u *(unsigned int *)($q + 56)
p/u *(unsigned int *)($q + 60)
p/u *(unsigned int *)($q + 64)
set $holder = *(unsigned int *)($q + 8)
p/x $holder
set $mutex_wait_list = $q + 36
w3e4_walk $mutex_wait_list 24
```

Kiểm tra `pcHead == 0`, `uxLength == 1`, `uxItemSize == 0`, holder hợp lệ và node `$waiter + 24` thật sự nằm trong wait list. Nếu holder khác `0`, đọc tên/priority/state/event container của holder rồi lặp lại để dựng cạnh `task -> mutex -> holder`. Với một symbol handle mutex, phải dereference handle; với symbol buffer `StaticQueue_t`, lấy địa chỉ buffer trực tiếp. Không mặc định semaphore nhị phân có holder hoặc priority inheritance như mutex.

### 4.5. Đáy stack, mẫu A5 và con trỏ ngữ cảnh

```gdb
set $stack_low = *(unsigned int *)($tcb + 48)
p/x $stack_low
x/16xw *(unsigned int *)((*(unsigned int *)&pxCurrentTCB) + 48)
x/64bx $stack_low
p/x *(unsigned int *)$tcb
info reg psp msp control
```

Lặp kiểm tra cho các task liên quan, không chỉ task hiện tại. Chỉ dump 64 byte khi đã biết vùng stack có ít nhất 64 byte. Xác định kích thước cấp phát từ symbol buffer stack (`nm -S`) hoặc tham số tạo task trong mã máy: `stack_bytes = depth_words * 4`, `stack_limit = stack_low + stack_bytes` (giới hạn trên không bao gồm). Nếu có `pxEndOfStack`, dùng như mốc high do port lưu và đối chiếu căn hàng; không lấy word `+68` làm mốc high ở biến thể A.

- Mẫu dự kiến là `0xA5A5A5A5`, nhưng chỉ khi đường khởi tạo có fill: `configCHECK_FOR_STACK_OVERFLOW > 1` hoặc `configUSE_TRACE_FACILITY == 1` hoặc một trong hai API high-water-mark được bật. `tasks.c` dùng `tskSTACK_FILL_BYTE = 0xA5` dưới các điều kiện này.
- Đếm số byte `0xA5` liên tục từ đáy lên, **có chặn tại biên stack**; `unused_words = unused_bytes / 4`. Đây là dấu vết vùng chưa bị ghi theo cơ chế fill, không đo chính xác mọi lần PSP từng đi qua.
- Với stack mọc xuống, PSP/ngữ cảnh lưu thấp hơn `stack_low`, ghi đè ra ngoài biên, hoặc exception frame bị phá là bằng chứng mạnh. Mất A5 ở đáy là tín hiệu cần điều tra; có thể là dùng hết stack hoặc ghi bậy từ nơi khác, không tự chứng minh tràn do call stack.
- Trong ISR, `$sp` thường là MSP. Phân biệt task bị ngắt (PSP) và stack handler; kiểm tra cả MSP nếu nghi tràn trong ISR. Với task đang chạy dùng PSP thực, với task đã được switch out dùng saved context; có thể bắt giữa PendSV nên phải đối chiếu lệnh tại PC.

### 4.6. Thanh ghi ngắt và đối chiếu priority

Interrupt Control and State Register (ICSR) ở `0xE000ED04`; Nested Vectored Interrupt Controller (NVIC) có các byte priority ngoài tại `0xE000E400`. Application Interrupt and Reset Control Register (AIRCR) ở `0xE000ED0C` chứa `PRIGROUP`.

```gdb
x/xw 0xE000ED04
x/16bx 0xE000E400
x/xw 0xE000ED0C
set $active = (*(unsigned int *)0xE000ED04) & 0x1ff
p/u $active
p/x $xpsr & 0x1ff
info reg basepri primask faultmask
if $active >= 16
  set $irqn = $active - 16
  p/u $irqn
  x/bx (0xE000E400 + $irqn)
  set $irq_priority = *(unsigned char *)(0xE000E400 + $irqn)
  p/x $irq_priority
end
```

`x/16bx 0xE000E400` chỉ đọc IRQn `0..15`. Ví dụ IRQn `37` phải đọc byte `0xE000E400 + 37`; không suy priority UART từ 16 byte đầu. Nếu `VECTACTIVE == 0`, CPU ở Thread mode, không có IRQ đang active tại ảnh chụp. Với exception hệ thống có priority cấu hình được (`4..15`, bỏ qua các mục reserved), đọc byte System Handler Priority tương ứng tại `0xE000ED18 + (exception_number - 4)`; HardFault/NMI có priority cố định, không tra NVIC IP bằng chỉ số âm.

Ngưỡng `$maxsys` phải lấy từ **chính ELF**: nếu còn symbol `ucMaxSysCallPriority`, sau khi scheduler đã khởi tạo dùng `p/x *(unsigned char *)&ucMaxSysCallPriority`; nếu không có, xem `vPortValidateInterruptPriority`, `vPortEnterCritical` hoặc lệnh ghi `BASEPRI` trong disassembly. `configMAX_SYSCALL_INTERRUPT_PRIORITY` là macro nên thường không có symbol để `p` khi mất DWARF. Không suy ngưỡng bằng `$basepri` đang bằng `0` ngoài critical section.

Với STM32F103 có 4 bit priority, đối chiếu trên cùng dạng byte thanh ghi: `priority_raw = priority_logical << (8 - 4)`. Ví dụ **chỉ nếu xác nhận ngưỡng logical là 5**, `$maxsys = 0x50`; ISR gọi API `...FromISR` phải có `priority_raw >= 0x50`, còn `0x00..0x40` vi phạm. Số nhỏ hơn biểu thị ưu tiên ngắt cao hơn. IRQ ưu tiên cao nhưng không gọi API FreeRTOS vẫn có thể hợp lệ. Quy tắc và cách shift được mô tả trong [FreeRTOS – Cortex-M interrupt priorities](https://www.freertos.org/Documentation/02-Kernel/03-Supported-devices/04-Demos/ARM-Cortex/RTOS-Cortex-M3-M4).

Khi `BASEPRI != 0`, nó chặn các ngắt có giá trị priority bằng/lớn hơn ngưỡng theo priority grouping; ngắt có số nhỏ hơn vẫn có thể chạy. Kiểm tra `AIRCR.PRIGROUP` để bảo đảm các bit priority triển khai được dành cho preemption theo port. `BASEPRI == 0` tại một lần halt không loại trừ lỗi; phải chứng minh ISR liên quan có đường gọi API kernel và đối chiếu priority của nó. Nếu halt trong HardFault, `VECTACTIVE` là HardFault, không phải IRQ gây lỗi trước đó; cần lần theo stacked frame/ngữ cảnh trước fault.

Nếu có fault, bổ sung:

```gdb
x/xw 0xE000ED28
x/xw 0xE000ED2C
x/2xw 0xE000ED34
x/8i $pc
bt
set logging enabled off
```

Đây lần lượt là Configurable Fault Status Register (CFSR), HardFault Status Register (HFSR), Memory Management Fault Address Register (MMFAR) và BusFault Address Register (BFAR). Chỉ diễn giải địa chỉ fault khi bit valid tương ứng có hiệu lực. Không dùng GDB `call` vào API kernel để lấy trạng thái vì việc thực thi sẽ thay đổi snapshot đang điều tra.

## 5. Bảng tiêu chí kết luận và loại trừ ba giả thuyết

Các ô dưới đây là **dấu hiệu dự kiến cần kiểm chứng**, không phải kết quả của `w3e4.elf`. Snapshot lỗi cần được chụp khi hệ thống đã chạy đến hiện tượng; snapshot ngay sau reset không đủ.

| Kênh bằng chứng | Deadlock | Stack Overflow | Sai Priority ISR |
|---|---|---|---|
| `$pc`, disassembly, `bt` | Có thể ở Idle vì các task liên quan đều block; task spinning cũng có thể giữ CPU. Tìm đường chờ mutex; PC ở Idle đơn lẻ không xác nhận vòng chờ. | Có thể ở `vApplicationStackOverflowHook`, HardFault, instruction/return address bất hợp lệ. Kiểm tra exception frame và biên stack; `bt` hỏng chỉ là dấu hỗ trợ. | Có thể ở `vPortValidateInterruptPriority`, vòng `configASSERT` hoặc ISR/API `...FromISR`. Dùng mã máy/call site xác nhận quan hệ ISR → API. |
| `pxCurrentTCB`, ready/blocked lists | Task A/B vắng khỏi ready list, state item ở delayed hoặc suspended list; đang chờ các khóa không được nhả. Current task có thể là Idle. | Task liên quan có SP/saved context vượt biên, tên/list links/owner bị ghi đè; cũng có thể list chưa hỏng. Xác minh layout trước khi gọi đó là corruption. | Có thể list bị sửa giữa critical section, assertion dừng trước khi hỏng, hoặc task state còn bình thường. Ready list không tự xác định priority ISR. |
| Event container / mutex holder | Chứng minh `A.event_container == &M2.receive_list`, `M2.holder == B`, `B.event_container == &M1.receive_list`, `M1.holder == A`, cùng membership thật. Đây là vòng `A -> M2 -> B -> M1 -> A`. | Container/holder có thể nằm ngoài SRAM, sai alignment hoặc không khớp liên kết; cần chứng minh liên quan vùng bị ghi đè, không diễn giải địa chỉ rác thành deadlock. | Container/holder có thể đúng hoặc sai do race. Không có vòng chờ hợp lệ giúp phản bác deadlock nhưng không tự xác nhận lỗi ISR. |
| Đáy stack `pxStack` / mẫu `0xA5A5A5A5` | Kỳ vọng stack các task liên quan còn margin, SP trong biên, context hợp lệ. Đây là bằng chứng chống tràn tại trạng thái đang xét, không bảo đảm mọi thời điểm trước đó. | Bằng chứng mạnh: SP/ngữ cảnh ra ngoài vùng cấp phát, frame bị phá hoặc xác định ghi vượt biên. Mất A5 khi đã xác nhận có fill hỗ trợ; A5 còn không loại trừ SP đi qua mà chưa ghi. | Stack task/ISR có thể vẫn hợp lệ. Stack hỏng cũng có thể là hậu quả của lỗi priority; cần nối chuỗi nguyên nhân bằng call site/ngữ cảnh. |
| `ICSR.VECTACTIVE`, `NVIC->IP[IRQn]`, `BASEPRI` | Không có ISR gọi API vi phạm ngưỡng; kiểm tra cả ISR có liên quan nhưng không active lúc halt. `VECTACTIVE == 0` riêng lẻ không loại trừ lỗi ISR. | Có thể đang ở HardFault; không gán priority HardFault thành priority ISR gây lỗi. Đối chiếu frame, CFSR/HFSR và vùng stack. | Xác nhận đúng IRQn, byte priority thực, ngưỡng lấy từ ELF và grouping. `priority_raw < maxsys` **đồng thời có gọi API kernel từ ISR** là vi phạm; kiểm tra API có hậu tố `FromISR`. |
| Điều kiện định lượng chốt giả thuyết | Có chu trình holder/waiter hợp lệ, mutex đang bị giữ (`uxMessagesWaiting == 0`), các task không tiến triển và không có cơ chế timeout/nhả khóa phá chu trình. | Có biên cấp phát đã biết và bằng chứng vượt/phá biên hoặc context; chỉ xác nhận không còn watermark thì chưa đủ xác định tác nhân ghi tràn. | Có phép so sánh priority raw với ngưỡng đúng, API call path và snapshot/assert phù hợp; chỉ thấy priority 0 ở một IRQ không dùng RTOS chưa đủ. |

### 5.1. Bằng chứng để phản bác từng giả thuyết còn lại

| Nếu nghiêng về | Cần phản bác giả thuyết thứ nhất | Cần phản bác giả thuyết thứ hai |
|---|---|---|
| Deadlock | Stack Overflow: kiểm tra biên, PSP/saved context, frame và watermark (nếu có fill) của tất cả task trong chu trình; không có dấu ghi vượt biên ở snapshot liên quan. | Sai Priority ISR: kiểm tra ISR dùng kernel, đúng API `FromISR`, priority raw đạt ngưỡng và grouping hợp lệ; không chỉ đọc IRQ đang active. |
| Stack Overflow | Deadlock: duyệt các task chờ mutex còn đọc được, xác nhận không có chu trình holder/waiter hợp lệ; nếu list đã hỏng không đủ dữ liệu để loại trừ bằng list. | Sai Priority ISR: đối chiếu các ISR gọi kernel và ngưỡng, xác nhận đường gây tràn từ call/exception context; một priority hợp lệ chưa bao quát các IRQ khác. |
| Sai Priority ISR | Deadlock: các task/khóa liên quan không tạo chu trình chờ hợp lệ, hoặc assertion xảy ra ngay trên đường ISR trước khi có vòng chờ. | Stack Overflow: biên task stack/MSP và context còn hợp lệ tại điểm bắt lỗi priority; nếu đã hỏng phải xác định thứ tự nguyên nhân, chưa được loại trừ bằng suy đoán. |

- Chờ mutex với timeout hữu hạn và sau đó thoát không phải deadlock vĩnh viễn. Nếu cần, giữ log snapshot đầu, cho chạy có kiểm soát rồi halt lần hai để so `xTickCount`, wake tick, holder và sự tiến triển; chỉ lặp tại cùng một điểm PC không đủ.
- Nếu dữ liệu thiếu để loại trừ một khả năng, ghi “chưa đủ bằng chứng” thay vì ép kết luận. Ba lỗi có thể cho triệu chứng giống nhau; hỏng list hoặc mất A5 không xác định được nguyên nhân khởi đầu.
- Với notification/semaphore/event group, không suy quan hệ holder như mutex. Với self-deadlock, kiểm tra task chờ chính mutex không recursive mà nó đang giữ; recursive mutex cần phân biệt bằng đường API và recursive count.

### 5.2. Bộ bằng chứng dự kiến vượt ngưỡng 6 lệnh

| Nhóm | Lệnh/output cần lưu nguyên văn | Giá trị cho kết luận |
|---|---|---|
| 1 | `info reg pc lr sp xpsr basepri`, thanh ghi MSP/PSP, `x/8i $pc`, `bt` | Thời điểm halt và ngữ cảnh thực thi |
| 2 | `x/512xw 0x20003300` | Cửa sổ 2048 byte được giao và đối chiếu object |
| 3 | `p/x *(unsigned int *)&pxCurrentTCB`, dump TCB đúng size, tên/priority | Task hiện tại và layout đã xác nhận |
| 4 | Dump header và các node ready/delayed/suspended/pending lists | Task Ready/Blocked và tính nhất quán liên kết |
| 5 | Event container, dump queue, holder và membership wait list | Đồ thị chờ khóa; xác nhận/loại trừ vòng chờ |
| 6 | Dump đáy stack, biên cấp phát, PSP/saved context và frame | Xác nhận/loại trừ dấu tràn stack |
| 7 | ICSR, byte priority đúng IRQn, AIRCR, BASEPRI, ngưỡng ELF, ISR disassembly | Xác nhận/loại trừ vi phạm priority/API ISR |
| 8, khi có fault | CFSR/HFSR/MMFAR/BFAR và stacked frame hợp lệ | Nối fault với stack/ISR/địa chỉ lỗi |

Đây là các nhóm bằng chứng, **không phải chỉ đếm 8 dòng bảng thành 8 lệnh**. Báo cáo sau này cần tối thiểu 6 câu lệnh GDB khác nhau có output thực, giải thích các giá trị và chuỗi suy luận; phép tính offset/DWARF/`nm` bổ trợ nhưng không thay bằng chứng RAM chạy thật.

### 5.3. Kiểm tra đã làm trong Chốt 1 và giới hạn

- Đã sinh đúng hai file tham số bằng `tools/gen_params.py --only w3e4`; không sinh lại tham số các bài khác.
- Đã biên dịch chính `tasks.c` và `queue.c` của repo thành object ARM Cortex-M3 ở thư mục tạm, với cấu hình kiểm tra tối thiểu đúng các giả định mục 2; dùng GDB batch đọc `sizeof` và địa chỉ trường tính từ con trỏ null, không kết nối target. Tổng 67 phép đối chiếu xác nhận 4 cấu hình TCB A/B0/B1/RECORD=0+TRACE=1 cùng các offset queue quan trọng.
- Đã thử object bỏ DWARF nhưng giữ symbol: kiểu cơ bản `unsigned int` và `void *` đều 4 byte, lấy địa chỉ symbol được; truy typedef `uint32_t` thất bại. Điều này kiểm tra lựa chọn cú pháp, không phải bằng chứng RAM của hộp đen.
- Đã chạy chính helper GDB trong tài liệu trên ELF mẫu tổng hợp không có DWARF ở thư mục tạm: duyệt đúng state list, event list, list rỗng và đọc được TCB/stack bằng cast kiểu cơ bản. Đây là kiểm tra cú pháp/thuật toán offline, không phải mô phỏng hay snapshot board W3.4.
- Các kiểm tra trên xác nhận bảng lý thuyết trong cấu hình kiểm tra, **không xác nhận layout hay loại lỗi của ELF Mentor**. Chưa flash, chưa chạy board, chưa có log GDB thực của W3.4; `REPORT.md` và `include/tcb_reference.h` giữ nguyên.

## 6. Chốt 2 – Layout thật của `w3e4.elf` (phân tích tĩnh, CHƯA chạy GDB trên board)

- File phân tích: `week3/ex4/w3e4.elf`, git blob `56114341e23c5c534d290492e149a6f9b360533b` (trùng `origin/main`), SHA-256 `118c9190af980404500b2c03ce3df0e09140d05117eee54cb735ee1a9a188b8e`.
- Công cụ: `arm-none-eabi-nm -S -n`, `arm-none-eabi-readelf -sW`, `arm-none-eabi-gdb -nx -batch` (chỉ đọc ELF, không kết nối target), `arm-none-eabi-objdump -d`.
- Trạng thái phần cứng lúc viết mục này: chưa cắm ST-Link (`openocd` báo `Error: open failed`). Mọi giá trị RAM ở mục 6.5 là **dự đoán**, chưa phải số đo.

### 6.1. Sai khác so với dự đoán Chốt 1

| Hạng mục | Dự đoán Chốt 1 | Thực tế của `w3e4.elf` | Nguồn |
|---|---|---|---|
| DWARF | Có thể đã bị strip | Còn đủ (`.debug_info` 26.871 byte); `ptype /o TCB_t` chạy được | `readelf -S`, GDB batch |
| `sizeof(TCB_t)` | Bảng chính `84 / 88 / 96`; `92` chỉ là ghi chú phụ | **`92 = 0x5C`**: `RECORD=0`, `TRACE=1`, có thêm `ucStaticallyAllocated` | `readelf -sW`: `s_task_a_tcb`, `s_task_b_tcb`, `s_idle_tcb`, `s_timer_tcb` đều `Size = 92` |
| Đuôi TCB | Phụ thuộc biến thể | `uxTCBNumber` 68, `uxTaskNumber` 72, `uxBasePriority` 76, **`uxMutexesHeld` 80**, `ulNotifiedValue` 84, `ucNotifyState` 88, `ucStaticallyAllocated` 89, padding 90..91 | `ptype /o TCB_t`; `pvTaskIncrementMutexHeldCount` có `ldr r3, [r3, #80]` và `str r2, [r3, #80]` |
| `pxEndOfStack` | Có điều kiện | **Không có**. Word `+68` là `uxTCBNumber` | `ptype /o TCB_t` |
| `sizeof(List_t)` | `20` (cần xác nhận) | `20`: `xListEnd` là `MiniListItem_t` 12 byte | `ptype /o List_t` |
| `configMAX_PRIORITIES` | Chưa chọn | `160 / 20 = 8`, dư `0`; `uxTopUsedPriority = 7` | `readelf -sW`: `pxReadyTasksLists` `Size = 160` |
| `sizeof(Queue_t)` | `72` | **`80`**: thêm `ucStaticallyAllocated` 70, `uxQueueNumber` 72, `ucQueueType` 76 | `ptype /o Queue_t`; `s_mutex1_storage`, `s_mutex2_storage` `Size = 80` |
| `xMutexHolder` | `+8` | `+8` (khớp). `+4` là `pcWriteTo` | `p &((Queue_t *)0)->u.xSemaphore.xMutexHolder` = `0x8` |
| `xTasksWaitingToReceive` | `+36` | `+36` (khớp) | `p &((Queue_t *)0)->xTasksWaitingToReceive` = `0x24` |
| Tên trường container | `pxContainer` (theo `list.h` của repo) | DWARF ghi **`pvContainer`**, vẫn ở offset 16 của `ListItem_t` | `ptype /o ListItem_t` |
| Hàm kế thừa priority | `xTaskPriorityInherit` | `xTaskPriorityInherit` tại `0x08001E08` (khớp); không có `vTaskPriorityInherit` | `nm` |

- Hai bẫy của `tcb_reference.h` đều có thật với ELF này: `List_t_ref` 28 byte sai (thật 20), và `pxEndOfStack` không tồn tại nên `uxBasePriority`/`uxMutexesHeld` trong header lệch 4 byte (header: 72/76, thật: 76/80).
- Vì DWARF còn đủ, script dùng thẳng kiểu `TCB_t`, `List_t`, `Queue_t` của chính ELF. Không dùng `TCB_t_ref`. Các lệnh `x/...xw` vẫn được giữ để đối chiếu byte thô.
- Tên trường `pvContainer` khác `list.h` trong `third_party/` của repo. Nghĩa là kernel Mentor dùng để build có thể không trùng từng dòng với bản trong repo. Chỉ tin layout đọc từ DWARF của ELF.

### 6.2. Bảng symbol trong SRAM (từ `nm -S -n`)

| Symbol | Địa chỉ | Size | Ghi chú |
|---|---:|---:|---|
| `s_timer_stack` / `s_timer_tcb` | `0x20000008` / `0x20000208` | 512 / 92 | Task `Tmr Svc`, priority 7 |
| `s_idle_stack` / `s_idle_tcb` | `0x20000264` / `0x20000464` | 512 / 92 | Task `IDLE` |
| `s_task_b_stack` / `s_task_b_tcb` | `0x200004C0` / `0x200006C0` | 512 / 92 | Task `task_b`, priority 1 |
| `s_task_a_stack` / `s_task_a_tcb` | `0x2000071C` / `0x2000091C` | 512 / 92 | Task `task_a`, priority 1 |
| `s_mutex2` / `s_mutex1` | `0x20000978` / `0x2000097C` | 4 / 4 | Handle (con trỏ tới storage) |
| `s_mutex2_storage` / `s_mutex1_storage` | `0x20000980` / `0x200009D0` | 80 / 80 | `Queue_t` tĩnh |
| `pxCurrentTCB` | `0x20000A60` | 4 | |
| `pxReadyTasksLists` | `0x20000A64` | 160 | 8 list, stride 20 |
| `xDelayedTaskList1` / `xDelayedTaskList2` | `0x20000B04` / `0x20000B18` | 20 / 20 | |
| `pxDelayedTaskList` / `pxOverflowDelayedTaskList` | `0x20000B2C` / `0x20000B30` | 4 / 4 | |
| `xPendingReadyList` | `0x20000B34` | 20 | |
| `xSuspendedTaskList` | `0x20000B60` | 20 | Có symbol nên `INCLUDE_vTaskSuspend = 1` |
| `uxCurrentNumberOfTasks` / `xTickCount` | `0x20000B74` / `0x20000B78` | 4 / 4 | |
| `ucHeap` | `0x20000C6C` | 4096 | |
| `ucMaxSysCallPriority` | `0x20001C80` | 1 | Ngưỡng syscall, đọc lúc chạy |
| `_ebss` / `_estack` | `0x20001C88` / `0x20005000` | — | |

- **Cửa sổ quét `0x20003300..0x20003AFF` không chứa symbol nào.** `.bss` kết thúc tại `0x20001C88`, `._user_heap_stack` tới `0x20002088`, stack MSP mọc xuống từ `0x20005000`. Dự đoán: `x/512xw 0x20003300` không cho thấy TCB hay list. Vùng này không được startup khởi tạo nên nội dung không xác định. Mọi object kernel nằm trong `0x20000008..0x20001C87`.

### 6.3. Thông tin đọc được từ mã máy

| Hàm | Nội dung | Suy ra |
|---|---|---|
| `main` (`0x08000354`) | 2 lần `xQueueCreateMutexStatic(1, …)`; 2 lần `xTaskCreateStatic` với stack depth `0x80`, priority `1`, tên tại `0x08002E0D` = `"task_a"`, `0x08002E14` = `"task_b"` | 2 mutex, 2 task cùng priority 1, stack `128 * 4 = 512` byte |
| `vTaskA` (`0x080002C8`) | `Take(s_mutex1, 0xFFFFFFFF)` → `vTaskDelay(100)` → `Take(s_mutex2, 0xFFFFFFFF)` → `Give(s_mutex2)` → `Give(s_mutex1)` → `vTaskDelay(100)` → lặp | Thứ tự khóa: mutex1 rồi mutex2 |
| `vTaskB` (`0x08000278`) | `vTaskDelay(20)` một lần, rồi `Take(s_mutex2, 0xFFFFFFFF)` → `vTaskDelay(100)` → `Take(s_mutex1, 0xFFFFFFFF)` → `Give` cả hai → `vTaskDelay(100)` → lặp | Thứ tự khóa ngược: mutex2 rồi mutex1 |
| `vPortSetupTimerInterrupt` | `SysTick LOAD = SystemCoreClock * 0x10624DD3 >> 38 − 1` | Tick `1000 Hz`; với `SystemCoreClock = 8.000.000` thì `LOAD = 7999` |
| `vTaskSwitchContext` | So 4 word đáy stack với `0xA5A5A5A5`, sai thì gọi `vApplicationStackOverflowHook` | `configCHECK_FOR_STACK_OVERFLOW = 2`, stack được fill `0xA5` |
| `vPortEnterCritical`, hook | `msr BASEPRI, #0x50` | Ngưỡng syscall raw `0x50` (logical 5 với 4 bit priority) |
| `g_vector_table` (59 word) | Chỉ Reset, SVC (`0x08002A01`), PendSV (`0x08002C11`), SysTick (`0x08002C59`) khác `Default_Handler` (`0x080003E1`) | Không có ISR ngoại vi nào của ứng dụng |
| Bảng symbol | Không có hàm `...FromISR`, không có `vPortValidateInterruptPriority` | Không có đường ISR → API kernel ngoài SysTick/PendSV/SVC |

### 6.4. Xếp hạng giả thuyết trước khi đọc RAM

| Hạng | Giả thuyết | Lý do từ ELF | Còn thiếu gì để kết luận |
|---|---|---|---|
| 1 | Deadlock | Hai task lấy hai mutex theo thứ tự ngược nhau, timeout vô hạn, có `vTaskDelay(100)` giữa hai lần `Take` nên cửa sổ đan chéo rộng 100 tick | Phải đọc RAM: holder của từng mutex, wait list, `pvContainer` của event item, và hai lần halt để thấy không tiến triển |
| 2 | Stack Overflow | Thấp: `vTaskA`/`vTaskB` chỉ `push {r3, lr}`, không có mảng cục bộ hay đệ quy; stack 512 byte | Phải đọc watermark `0xA5A5A5A5` và `pxTopOfStack` của cả 4 task |
| 3 | Sai priority ISR | Thấp: không có ISR ngoại vi, không có API `FromISR` | Phải đọc `NVIC ISER`, `NVIC IP`, `SHPR`, `ICSR`, `ucMaxSysCallPriority` lúc chạy |

- Mã máy chỉ cho biết đường đi có thể xảy ra. Nó không cho biết trạng thái thật trên board. Kết luận chỉ được viết sau khi có log GDB.

### 6.5. Giá trị RAM dự đoán khi halt sau reset ≥ 200 ms

Dòng thời gian dự đoán (tick 1 ms): tick 0 `task_a` lấy mutex1; tick 20 `task_b` lấy mutex2; tick 100 `task_a` chờ mutex2; tick 120 `task_b` chờ mutex1. Từ tick 120 không task ứng dụng nào chạy nữa.

| Biểu thức GDB | Giá trị dự đoán | Phép tính |
|---|---|---|
| `pxCurrentTCB` | `0x20000464` (`s_idle_tcb`, tên `IDLE`) | Chỉ còn Idle ở trạng thái Ready |
| `((Queue_t *)s_mutex1)->u.xSemaphore.xMutexHolder` | `0x2000091C` (`task_a`) | `&s_task_a_tcb` |
| `((Queue_t *)s_mutex2)->u.xSemaphore.xMutexHolder` | `0x200006C0` (`task_b`) | `&s_task_b_tcb` |
| `uxMessagesWaiting` của mutex1, mutex2 | `0`, `0` | Cả hai đang bị giữ |
| Node trong wait list của mutex1 | `0x200006D8` | `&s_task_b_tcb + 24` |
| Node trong wait list của mutex2 | `0x20000934` | `&s_task_a_tcb + 24` |
| `task_a.xEventListItem.pvContainer` | `0x200009A4` | `&s_mutex2_storage + 36` |
| `task_b.xEventListItem.pvContainer` | `0x200009F4` | `&s_mutex1_storage + 36` |
| `uxMutexesHeld` của `task_a`, `task_b` | `1`, `1` | Mỗi task giữ đúng 1 mutex |
| `uxPriority` / `uxBasePriority` của `task_a`, `task_b` | `1 / 1` | Cùng priority nên không có kế thừa |
| `xSuspendedTaskList.uxNumberOfItems` | `3` (`task_a`, `task_b`, `Tmr Svc`) | Chờ `portMAX_DELAY` thì state item vào suspended list, không vào delayed list |
| `pxDelayedTaskList`, `pxOverflowDelayedTaskList` | Rỗng | |
| `pxReadyTasksLists[0]` / `[1..7]` | `1` (`IDLE`) / `0` | |
| 4 word đáy stack của cả 4 task | `0xA5A5A5A5` | Không có tràn |
| `NVIC ISER0..1` | `0`, `0` | Không bật IRQ ngoại vi |
| Giữa hai lần halt | `xTickCount` tăng; holder và `pxTopOfStack` của `task_a`, `task_b` không đổi | Hệ thống còn tick nhưng hai task không tiến triển |

### 6.6. Kịch bản chạy và số lệnh

- Script: `week3/ex4/debug_w3e4.gdb`. Chạy: `arm-none-eabi-gdb -nx -batch -x week3/ex4/debug_w3e4.gdb week3/ex4/w3e4.elf`. Log: `week3/ex4/w3e4-gdb.log`.
- Script có 7 nhóm bằng chứng `L1..L7` và hai ảnh chụp ngắn `S1`, `S2` cách nhau `$run_ms` (mặc định 2000 ms). Đổi thời điểm halt bằng `-ex "set $run_ms = 5000"` đặt trước `-x`.
- Số lệnh có output dự kiến: khoảng 60, cao hơn mức `18..30` của Chốt 1 vì phải duyệt 8 mức priority và 4 task.
- Kiểm tra đã làm: chạy script ở chế độ offline (`$w3e4_offline`) trên một bản sao ELF có `.bss` giả lập để thử cú pháp và helper duyệt list. Đây **không** phải dữ liệu board và không được dùng làm bằng chứng trong `REPORT.md`. Phần kết nối OpenOCD, `load`, `monitor` và đọc thanh ghi chưa chạy được vì chưa có board.
