# W3.4 – REPORT

## 0. Điều kiện đo

| Mục | Giá trị |
|---|---|
| Firmware | `week3/ex4/w3e4.elf`, git blob `56114341e23c5c534d290492e149a6f9b360533b`, không sửa |
| Phần cứng | STM32F103 qua ST-Link, OpenOCD `xPack 0.12.0+dev-02228` (`interface/stlink.cfg`, `target/stm32f1x.cfg`) |
| GDB | `arm-none-eabi-gdb` 15.2.90 (Arm GNU Toolchain 14.2.Rel1) |
| Script | `week3/ex4/debug_w3e4.gdb` (chỉ đọc RAM và thanh ghi, không `call`, không ghi RAM) |
| Lệnh chạy | `arm-none-eabi-gdb -nx -batch -x week3/ex4/debug_w3e4.gdb week3/ex4/w3e4.elf` |
| Log nguyên văn | `week3/ex4/w3e4-gdb.log` (583 dòng, 84 lệnh GDB có output) |
| Trình tự | `monitor reset halt` → `load` → `compare-sections` (4/4 `matched`) → `monitor reset halt` → hw breakpoint tại `Default_Handler` và `vApplicationStackOverflowHook` → chạy 2000 ms → halt **lần 1** → đọc toàn bộ → chạy thêm 2000 ms → halt **lần 2** |
| Thời điểm halt | Lần 1: `xTickCount = 2033`. Lần 2: `xTickCount = 4049` |

Ba dòng đầu log là trạng thái board **trước** `load` (GDB attach vào firmware cũ đang chạy). Báo cáo không dùng chúng. Mọi số liệu bên dưới lấy sau `load` và `reset halt`.

Layout dùng để đọc RAM lấy từ DWARF của chính ELF (`ptype /o`), không dùng `tcb_reference.h`: `sizeof(TCB_t) = 92`, `sizeof(List_t) = 20`, `sizeof(Queue_t) = 80`, `xMutexHolder` tại `Queue_t + 8`, `xTasksWaitingToReceive` tại `Queue_t + 36`, `uxMutexesHeld` tại `TCB + 80`. Chi tiết ở `PREDICTION.md` mục 6.1.

## 1. Số đo (bằng chứng GDB)

Tối thiểu `W3E4_MIN_GDB_EVIDENCE_COMMANDS = 6` lệnh GDB khác nhau. Bảng dưới có 14 lệnh. Output nguyên văn đầy đủ của từng lệnh nằm ở mục 2.

| # | Lệnh GDB | Output (nguyên văn, rút gọn hợp lý) | Kết luận rút ra từ lệnh này |
|---|---|---|---|
| 1 | `x/512xw 0x20003300` | 512 word, ví dụ `0x20003300: 0xdb68fa7f 0x340e73c1 0x601f4145 0x607b41ab` | Cửa sổ PARAMS không chứa object kernel: 0/512 word trỏ vào `.bss`, 0/512 word là `0xa5a5a5a5`, 0/512 word trỏ vào code. Đây là RAM chưa khởi tạo. Object thật nằm ở `0x20000008..0x20001C87` |
| 2 | `info all-registers` | `pc 0x8001cf0 <prvCheckTasksWaitingTermination>`, `psp 0x20000450 <s_idle_stack+492>`, `control 0x2`, `basepri 0x0`, `primask 0x0` | CPU ở Thread mode, chạy task Idle trên PSP. Không ở HardFault, không ở hook |
| 3 | `backtrace` | `#0 prvCheckTasksWaitingTermination` / `#1 prvIdleTask` / `#2 pxPortInitialiseStack` | Đường gọi bình thường của Idle. Frame #2 là địa chỉ trả về giả do port đặt khi tạo task, không phải stack hỏng |
| 4 | `p *pxCurrentTCB` | `pcTaskName = "IDLE"`, `uxPriority = 0`, `xStateListItem.pvContainer = 0x20000a64 <pxReadyTasksLists>` | Task đang chạy là Idle |
| 5 | `p xTickCount` / `p uxCurrentNumberOfTasks` / `p xSchedulerRunning` | `2033` / `4` / `1` | Scheduler chạy, tick tăng, có 4 task |
| 6 | `x/5xw &pxReadyTasksLists[i]`, `i = 0..7` | `[0]: 0x00000001 0x20000468 ...`; `[1..7]: 0x00000000` | Ready list chỉ có 1 task: `IDLE` (node `0x20000468 = s_idle_tcb + 4`) |
| 7 | `x/5xw &xSuspendedTaskList` | `0x00000003 0x20000b68 0xffffffff 0x2000020c 0x200006c4` | 3 task bị block vô hạn: `Tmr Svc`, `task_a`, `task_b`. Delayed và overflow list đều rỗng |
| 8 | `x/20xw 0x200009d0` (`s_mutex1`) | `0x00000000 0x200009d0 0x2000091c ...` / `+36: 0x00000001 ... 0x200006d8` / `+56: 0x00000000 0x00000001` | `mutex1.holder = 0x2000091c = task_a`; wait list có 1 node `0x200006d8 = task_b + 24`; `uxMessagesWaiting = 0` (đang bị giữ) |
| 9 | `x/20xw 0x20000980` (`s_mutex2`) | `0x00000000 0x20000980 0x200006c0 ...` / `+36: 0x00000001 ... 0x20000934` / `+56: 0x00000000 0x00000001` | `mutex2.holder = 0x200006c0 = task_b`; wait list có 1 node `0x20000934 = task_a + 24`; `uxMessagesWaiting = 0` |
| 10 | `x/16xw <pxStack>` cho 4 task | Cả 4 vùng: 16/16 word `0xa5a5a5a5` | Watermark còn nguyên. Còn 376 / 372 / 440 / 332 byte chưa từng bị ghi (`task_a` / `task_b` / `IDLE` / `Tmr Svc`) trên 512 byte |
| 11 | `x/xw 0xE000ED04` (ICSR) | `0xe000ed04: 0x00000000` | `VECTACTIVE = 0`: không có exception nào đang active |
| 12 | `x/2xw 0xE000E100` (NVIC ISER0..1) | `0xe000e100: 0x00000000 0x00000000` | Không có IRQ ngoại vi nào được bật |
| 13 | `x/3xw 0xE000ED18` (SHPR1..3) / `p/x ucMaxSysCallPriority` | `0x00000000 0x00000000 0xf0f00000` / `$25 = 0x50` | PendSV = SysTick = `0xF0` ≥ ngưỡng `0x50`: hợp lệ |
| 14 | `x/xw 0xE000ED28` (CFSR) / `x/xw 0xE000ED2C` (HFSR) | `0x00000000` / `0x00000000` | Không có fault nào được ghi nhận |
| S | Ảnh chụp ngắn lần 1 và lần 2 (`p xTickCount`, `p/x ...->pxTopOfStack`, `p/x ...->xMutexHolder`) | Tick `2033 → 4049`; mọi giá trị khác không đổi | Hệ thống còn tick nhưng `task_a`, `task_b` không tiến triển trong 2016 tick |

## 2. Output nguyên văn theo nhóm

### 2.1. [L1] Quét cửa sổ SRAM của PARAMS (`0x20003300..0x20003AFF`)

```text
+info symbol 0x20003300
No symbol matches 0x20003300.
+info symbol 0x20003AFF
No symbol matches 0x20003AFF.
+p/x &_ebss
$1 = 0x20001c88
+p/x &_estack
$2 = 0x20005000
++x/512xw 0x20003300
0x20003300:	0xdb68fa7f	0x340e73c1	0x601f4145	0x607b41ab
0x20003310:	0xe22f30f1	0x541bbce5	0x451c288c	0xa89b0aa0
0x20003320:	0x79be3aca	0xfd5cc785	0xd453d045	0xb2ae1b0d
...                         (124 dòng giữa, xem w3e4-gdb.log dòng 51..174)
0x20003af0:	0x65bff51c	0x298f2438	0xa1a882f9	0xc18fb841
```

Phân loại 512 word của cửa sổ (đếm bằng script trên log):

| Loại giá trị | Số word |
|---|---:|
| Trỏ vào `.bss` (`0x20000000..0x20001C87`) | 0 |
| Bằng `0xA5A5A5A5` (mẫu fill stack) | 0 |
| Trỏ vào code (`0x08000000..0x08002E2F`) | 0 |

Kết luận: cửa sổ nằm giữa `._user_heap_stack` (hết ở `0x20002088`) và vùng MSP (`msp = 0x20004fe0`). Startup không ghi vùng này, nên giá trị là RAM ngẫu nhiên lúc cấp nguồn. Không có TCB, list hay mutex trong cửa sổ. Đối chiếu bảng `nm` (mục 3) thì mọi object kernel nằm ở `0x20000008..0x20001C87`.

### 2.2. [L2] CPU và task đang chạy

```text
++info registers pc lr sp
pc             0x8001cf0           0x8001cf0 <prvCheckTasksWaitingTermination>
lr             0x8001c4d           134224973
sp             0x20000450          0x20000450 <s_idle_stack+492>
++info all-registers
...
xpsr           0x61000000          1627389952
msp            0x20004fe0          0x20004fe0
psp            0x20000450          0x20000450 <s_idle_stack+492>
primask        0x0                 0
basepri        0x0                 0
faultmask      0x0                 0
control        0x2                 2
++backtrace
#0  prvCheckTasksWaitingTermination () at .../FreeRTOS-Kernel/tasks.c:6044
#1  0x08001c4c in prvIdleTask (pvParameters=0x0) at .../FreeRTOS-Kernel/tasks.c:5773
#2  0x080029b0 in pxPortInitialiseStack (pxTopOfStack=0x0, pxCode=0xa5a5a5a5, pvParameters=0x8001c4d <prvIdleTask+12>) at .../portable/GCC/ARM_CM3/port.c:199
Backtrace stopped: previous frame identical to this frame (corrupt stack?)
+p/x pxCurrentTCB
$3 = 0x20000464
+info symbol pxCurrentTCB
s_idle_tcb in section .bss
++p *pxCurrentTCB
$4 = {
  pxTopOfStack = 0x20000410 <s_idle_stack+428>,
  xStateListItem = { xItemValue = 0, pxNext = 0x20000a6c <pxReadyTasksLists+8>,
    pxPrevious = 0x20000a6c <pxReadyTasksLists+8>, pvOwner = 0x20000464 <s_idle_tcb>,
    pvContainer = 0x20000a64 <pxReadyTasksLists> },
  xEventListItem = { xItemValue = 8, pxNext = 0x0, pxPrevious = 0x0,
    pvOwner = 0x20000464 <s_idle_tcb>, pvContainer = 0x0 },
  uxPriority = 0,
  pxStack = 0x20000264 <s_idle_stack>,
  pcTaskName = "IDLE", '\000' <repeats 11 times>,
  uxTCBNumber = 3, uxTaskNumber = 0, uxBasePriority = 0, uxMutexesHeld = 0,
  ulNotifiedValue = {0}, ucNotifyState = "", ucStaticallyAllocated = 2 '\002'
}
+p xTickCount
$5 = 2033
+p uxCurrentNumberOfTasks
$6 = 4
+p uxTopReadyPriority
$7 = 0
+p xSchedulerRunning
$8 = 1
+p uxSchedulerSuspended
$9 = 0
```

- `control = 0x2` (bit SPSEL = 1): Thread mode dùng PSP. `xpsr` có trường exception number = 0. CPU không ở trong handler.
- `psp = 0x20000450` nằm trong `s_idle_stack` (`0x20000264..0x20000463`).
- Dòng `corrupt stack?` của `backtrace` **không** phải bằng chứng tràn. Frame #2 là địa chỉ trả về giả (`prvTaskExitError`, `0x080029b0`) mà `pxPortInitialiseStack` đặt vào đáy stack khi tạo task. Đây là đáy chuỗi gọi của mọi task FreeRTOS. Mục 2.6 chứng minh stack còn nguyên.
- `uxTopReadyPriority = 0`: priority Ready cao nhất là 0, tức chỉ có Idle sẵn sàng chạy.

### 2.3. [L3] Ready list theo từng priority (stride 20 byte)

```text
+p sizeof(List_t)
$10 = 20
+p sizeof(pxReadyTasksLists)
$11 = 160
+p $nprio
$12 = 8
(gdb) x/5xw &pxReadyTasksLists[0]    dia chi 0x20000a64 = base + 20*0
0x20000a64 <pxReadyTasksLists>:	0x00000001	0x20000468	0xffffffff	0x20000468
0x20000a74 <pxReadyTasksLists+16>:	0x20000468
list=0x20000a64 uxNumberOfItems=1 pxIndex=0x20000468 sentinel=0x20000a6c
  node=0x20000468 xItemValue=0 pvOwner=0x20000464 pvContainer=0x20000a64 -> task "IDLE" uxPriority=0
  visited=1 declared=1
(gdb) x/5xw &pxReadyTasksLists[1]    dia chi 0x20000a78 = base + 20*1
0x20000a78 <pxReadyTasksLists+20>:	0x00000000	0x20000a80	0xffffffff	0x20000a80
0x20000a88 <pxReadyTasksLists+36>:	0x20000a80
list=0x20000a78 uxNumberOfItems=0 pxIndex=0x20000a80 sentinel=0x20000a80
  visited=0 declared=0
...                         ([2]..[6] giống [1]: uxNumberOfItems = 0, sentinel trỏ về chính nó)
(gdb) x/5xw &pxReadyTasksLists[7]    dia chi 0x20000af0 = base + 20*7
0x20000af0 <pxReadyTasksLists+140>:	0x00000000	0x20000af8	0xffffffff	0x20000af8
0x20000b00 <pxReadyTasksLists+156>:	0x20000af8
list=0x20000af0 uxNumberOfItems=0 pxIndex=0x20000af8 sentinel=0x20000af8
  visited=0 declared=0
```

- Stride 20 byte đúng: mọi list rỗng có `xListEnd.xItemValue = 0xffffffff` ở `base + 20*i + 8`, và `pxNext = pxPrevious = sentinel = base + 20*i + 8`. Nếu dùng stride 28 của `tcb_reference.h`, list `[1]` sẽ rơi vào `0x20000a80` (sentinel của `[1]`), sai từ mức đầu tiên.
- `task_a` và `task_b` (priority 1) **không** có trong `pxReadyTasksLists[1]`.

### 2.4. [L4] Danh sách Blocked

```text
+p/x pxDelayedTaskList
$13 = 0x20000b04
list=0x20000b04 uxNumberOfItems=0 pxIndex=0x20000b0c sentinel=0x20000b0c
  visited=0 declared=0
+p/x pxOverflowDelayedTaskList
$14 = 0x20000b18
list=0x20000b18 uxNumberOfItems=0 pxIndex=0x20000b20 sentinel=0x20000b20
  visited=0 declared=0
+x/5xw &xSuspendedTaskList
0x20000b60 <xSuspendedTaskList>:	0x00000003	0x20000b68	0xffffffff	0x2000020c
0x20000b70 <xSuspendedTaskList+16>:	0x200006c4
list=0x20000b60 uxNumberOfItems=3 pxIndex=0x20000b68 sentinel=0x20000b68
  node=0x2000020c xItemValue=0 pvOwner=0x20000208 pvContainer=0x20000b60 -> task "Tmr Svc" uxPriority=7
  node=0x20000920 xItemValue=0x64 pvOwner=0x2000091c pvContainer=0x20000b60 -> task "task_a" uxPriority=1
  node=0x200006c4 xItemValue=0x78 pvOwner=0x200006c0 pvContainer=0x20000b60 -> task "task_b" uxPriority=1
  visited=3 declared=3
+x/5xw &xPendingReadyList
0x20000b34 <xPendingReadyList>:	0x00000000	0x20000b3c	0xffffffff	0x20000b3c
0x20000b44 <xPendingReadyList+16>:	0x20000b3c
list=0x20000b34 uxNumberOfItems=0 pxIndex=0x20000b3c sentinel=0x20000b3c
  visited=0 declared=0
```

- Delayed list và overflow delayed list đều rỗng. Không task nào đang chờ có thời hạn.
- `task_a`, `task_b` nằm trong `xSuspendedTaskList`. Với `INCLUDE_vTaskSuspend = 1`, `prvAddCurrentTaskToDelayedList` đưa task chờ `portMAX_DELAY` vào list này. Đây là chờ **vô hạn**, không có timeout nào phá vòng chờ.
- Số liệu định lượng: `xItemValue` của state item giữ wake tick của lần `vTaskDelay` cuối trước khi block.
  - `task_a`: `0x64 = 100`. Task thức ở tick 100 sau `vTaskDelay(100)`, gọi `Take(mutex2)` và block.
  - `task_b`: `0x78 = 120 = 20 + 100`. Task thức ở tick 120 sau `vTaskDelay(20)` và `vTaskDelay(100)`, gọi `Take(mutex1)` và block.
  - Hai số này khớp đúng dòng thời gian dự đoán ở `PREDICTION.md` mục 6.5.
- `Tmr Svc` chờ vô hạn trên `xStaticTimerQueue.0 + 36` (mục 2.6). Đây là hành vi bình thường của task timer khi không có lệnh timer, không phải một phần của lỗi.

### 2.5. [L5] Giả thuyết 1 – Deadlock: holder và waiter của hai mutex

```text
+p/x s_mutex1
$15 = 0x200009d0
+p/x s_mutex2
$16 = 0x20000980
+p &((Queue_t *)0)->u.xSemaphore.xMutexHolder
$17 = (TaskHandle_t *) 0x8
+p &((Queue_t *)0)->xTasksWaitingToReceive
$18 = (List_t *) 0x24
+p &((TCB_t *)0)->uxMutexesHeld
$19 = (UBaseType_t *) 0x50
Queue_t=0x200009d0 : s_mutex1_storage in section .bss
  pcHead(+0)=0 pcWriteTo(+4)=0x200009d0 xMutexHolder(+8)=0x2000091c uxRecursiveCallCount(+12)=0
  uxMessagesWaiting(+56)=0 uxLength(+60)=1 uxItemSize(+64)=0 ucQueueType(+76)=1
  (gdb) x/20xw 0x200009d0
0x200009d0 <s_mutex1_storage>:	0x00000000	0x200009d0	0x2000091c	0x00000000
0x200009e0 <s_mutex1_storage+16>:	0x00000000	0x200009e8	0xffffffff	0x200009e8
0x200009f0 <s_mutex1_storage+32>:	0x200009e8	0x00000001	0x200009fc	0xffffffff
0x20000a00 <s_mutex1_storage+48>:	0x200006d8	0x200006d8	0x00000000	0x00000001
0x20000a10 <s_mutex1_storage+64>:	0x00000000	0x0001ffff	0x00000000	0x00000001
  holder -> task "task_a" uxMutexesHeld=1
  xTasksWaitingToReceive(+36):
list=0x200009f4 uxNumberOfItems=1 pxIndex=0x200009fc sentinel=0x200009fc
  node=0x200006d8 xItemValue=0x7 pvOwner=0x200006c0 pvContainer=0x200009f4 -> task "task_b" uxPriority=1
  visited=1 declared=1
Queue_t=0x20000980 : s_mutex2_storage in section .bss
  pcHead(+0)=0 pcWriteTo(+4)=0x20000980 xMutexHolder(+8)=0x200006c0 uxRecursiveCallCount(+12)=0
  uxMessagesWaiting(+56)=0 uxLength(+60)=1 uxItemSize(+64)=0 ucQueueType(+76)=1
  (gdb) x/20xw 0x20000980
0x20000980 <s_mutex2_storage>:	0x00000000	0x20000980	0x200006c0	0x00000000
0x20000990 <s_mutex2_storage+16>:	0x00000000	0x20000998	0xffffffff	0x20000998
0x200009a0 <s_mutex2_storage+32>:	0x20000998	0x00000001	0x200009ac	0xffffffff
0x200009b0 <s_mutex2_storage+48>:	0x20000934	0x20000934	0x00000000	0x00000001
0x200009c0 <s_mutex2_storage+64>:	0x00000000	0x0001ffff	0x00000000	0x00000001
  holder -> task "task_b" uxMutexesHeld=1
  xTasksWaitingToReceive(+36):
list=0x200009a4 uxNumberOfItems=1 pxIndex=0x200009ac sentinel=0x200009ac
  node=0x20000934 xItemValue=0x7 pvOwner=0x2000091c pvContainer=0x200009a4 -> task "task_a" uxPriority=1
  visited=1 declared=1
```

Kiểm tra đây đúng là mutex trước khi đọc union như mutex:

| Điều kiện | `s_mutex1` | `s_mutex2` |
|---|---|---|
| `pcHead == NULL` (`queueQUEUE_IS_MUTEX`) | `0x00000000` | `0x00000000` |
| `uxLength == 1`, `uxItemSize == 0` | `1`, `0` | `1`, `0` |
| `ucQueueType == 1` (`queueQUEUE_TYPE_MUTEX`) | `1` | `1` |
| `uxRecursiveCallCount == 0` (không phải recursive) | `0` | `0` |
| `uxMessagesWaiting == 0` (đang bị giữ) | `0` | `0` |
| `xMutexHolder` (+8) | `0x2000091c` = `task_a` | `0x200006c0` = `task_b` |
| Node trong `xTasksWaitingToReceive` (+36) | `0x200006d8` = `task_b + 24` | `0x20000934` = `task_a + 24` |
| `xItemValue` của event item | `7` = `configMAX_PRIORITIES − 1` = `8 − 1` | `7` |

Đối chiếu ngược từ phía TCB (output mục 2.6):

```text
TCB=0x2000091c name="task_a" uxPriority=1 uxBasePriority=1 uxMutexesHeld=1
  xStateListItem.pvContainer=0x20000b60 : xSuspendedTaskList in section .bss
  xEventListItem.pvContainer=0x200009a4 : s_mutex2_storage + 36 in section .bss
TCB=0x200006c0 name="task_b" uxPriority=1 uxBasePriority=1 uxMutexesHeld=1
  xStateListItem.pvContainer=0x20000b60 : xSuspendedTaskList in section .bss
  xEventListItem.pvContainer=0x200009f4 : s_mutex1_storage + 36 in section .bss
```

Đồ thị chờ dựng từ 4 giá trị đọc được:

```text
task_a ──giữ──▶ s_mutex1 (holder = 0x2000091c)
task_a ──chờ──▶ s_mutex2 (task_a.event.pvContainer = 0x200009a4 = &s_mutex2->xTasksWaitingToReceive)
task_b ──giữ──▶ s_mutex2 (holder = 0x200006c0)
task_b ──chờ──▶ s_mutex1 (task_b.event.pvContainer = 0x200009f4 = &s_mutex1->xTasksWaitingToReceive)

task_a → s_mutex2 → task_b → s_mutex1 → task_a   (chu trình đóng)
```

- Liên kết hai chiều khớp: holder trong `Queue_t` trỏ đúng TCB, `pvContainer` trong TCB trỏ đúng wait list, và wait list chứa đúng node `TCB + 24`.
- `uxMutexesHeld = 1` ở cả hai task: mỗi task giữ đúng 1 mutex.
- `uxPriority == uxBasePriority == 1` ở cả hai task: cùng priority nên priority inheritance không có gì để nâng, và cũng không thể phá vòng chờ.

### 2.6. [L6] Giả thuyết 2 – Stack overflow: mọi task, kể cả IDLE

```text
TCB=0x2000091c name="task_a" uxPriority=1 uxBasePriority=1 uxMutexesHeld=1
  pxStack=0x2000071c limit=0x2000091c pxTopOfStack=0x200008a0 con_trong_luc_luu_ngu_canh=388 byte
  (gdb) x/16xw 0x2000071c
0x2000071c <s_task_a_stack>:	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5
0x2000072c <s_task_a_stack+16>:	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5
0x2000073c <s_task_a_stack+32>:	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5
0x2000074c <s_task_a_stack+48>:	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5
  watermark: 94 word 0xA5A5A5A5 lien tuc tu day = 376 byte chua tung bi ghi
TCB=0x200006c0 name="task_b" uxPriority=1 uxBasePriority=1 uxMutexesHeld=1
  pxStack=0x200004c0 limit=0x200006c0 pxTopOfStack=0x20000640 con_trong_luc_luu_ngu_canh=384 byte
  (gdb) x/16xw 0x200004c0
0x200004c0 <s_task_b_stack>:	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5
...                         (3 dòng còn lại đều 0xa5a5a5a5)
  watermark: 93 word 0xA5A5A5A5 lien tuc tu day = 372 byte chua tung bi ghi
TCB=0x20000464 name="IDLE" uxPriority=0 uxBasePriority=0 uxMutexesHeld=0
  pxStack=0x20000264 limit=0x20000464 pxTopOfStack=0x20000410 con_trong_luc_luu_ngu_canh=428 byte
  (gdb) x/16xw 0x20000264
0x20000264 <s_idle_stack>:	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5
...                         (3 dòng còn lại đều 0xa5a5a5a5)
  watermark: 110 word 0xA5A5A5A5 lien tuc tu day = 440 byte chua tung bi ghi
TCB=0x20000208 name="Tmr Svc" uxPriority=7 uxBasePriority=7 uxMutexesHeld=0
  xEventListItem.pvContainer=0x20000c40 : xStaticTimerQueue.0 + 36 in section .bss
  pxStack=0x20000008 limit=0x20000208 pxTopOfStack=0x20000190 con_trong_luc_luu_ngu_canh=392 byte
  (gdb) x/16xw 0x20000008
0x20000008 <s_timer_stack>:	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5	0xa5a5a5a5
...                         (3 dòng còn lại đều 0xa5a5a5a5)
  watermark: 83 word 0xA5A5A5A5 lien tuc tu day = 332 byte chua tung bi ghi
+p xNumOfOverflows
$20 = 0
```

| Task | `pxStack` | Giới hạn trên (`pxStack + 512`) | `pxTopOfStack` | Trong biên? | Byte còn tới đáy | Watermark A5 | Đã dùng tối đa |
|---|---|---|---|---|---:|---:|---:|
| `task_a` | `0x2000071C` | `0x2000091C` | `0x200008A0` | Có | 388 | 376 byte (94 word) | 136 byte (27%) |
| `task_b` | `0x200004C0` | `0x200006C0` | `0x20000640` | Có | 384 | 372 byte (93 word) | 140 byte (27%) |
| `IDLE` | `0x20000264` | `0x20000464` | `0x20000410` | Có | 428 | 440 byte (110 word) | 72 byte (14%) |
| `Tmr Svc` | `0x20000008` | `0x20000208` | `0x20000190` | Có | 392 | 332 byte (83 word) | 180 byte (35%) |

- `IDLE` đang chạy nên `pxTopOfStack` là giá trị lưu lần trước. Giá trị thật lúc halt là `psp = 0x20000450`, vẫn trong `0x20000264..0x20000463`.
- ELF có `configCHECK_FOR_STACK_OVERFLOW = 2` (`vTaskSwitchContext` so 4 word đáy với `0xA5A5A5A5`, xem `PREDICTION.md` mục 6.3). Hardware breakpoint tại `vApplicationStackOverflowHook` (`0x08000310`) được đặt thành công (`breakpoint set at 0x08000310`) nhưng **không** bị chạm trong 4049 tick.

### 2.7. [L7] Giả thuyết 3 – Sai priority ISR

```text
++x/xw 0xE000ED04
0xe000ed04:	0x00000000
++p/u (*(unsigned int *)0xE000ED04) & 0x1ff
$21 = 0
++x/2xw 0xE000E100
0xe000e100:	0x00000000	0x00000000
++x/32bx 0xE000E400
0xe000e400:	0x00	0x00	0x00	0x00	0x00	0x00	0x00	0x00
...                         (3 dòng còn lại đều 0x00)
++x/28bx 0xE000E420
0xe000e420:	0x00	0x00	0x00	0x00	0x00	0x00	0x00	0x00
...                         (đều 0x00, tới 0xe000e43b)
++x/3xw 0xE000ED18
0xe000ed18:	0x00000000	0x00000000	0xf0f00000
++x/xw 0xE000ED0C
0xe000ed0c:	0xfa050300
++p/x $basepri
$22 = 0x0
++p/x $primask
$23 = 0x0
++p/x $xpsr
$24 = 0x61000000
++x/xw 0xE000ED28
0xe000ed28:	0x00000000
++x/xw 0xE000ED2C
0xe000ed2c:	0x00000000
+p/x ucMaxSysCallPriority
$25 = 0x50
+p/x ulMaxPRIGROUPValue
$26 = 0x300
+info functions FromISR
All functions matching regular expression "FromISR":
+x/59xw &g_vector_table
0x8000000 <g_vector_table>:	0x20005000	0x080003e5	0x080003e1	0x080003e1
0x8000010 <g_vector_table+16>:	0x080003e1	0x080003e1	0x080003e1	0x00000000
0x8000020 <g_vector_table+32>:	0x00000000	0x00000000	0x00000000	0x08002a01
0x8000030 <g_vector_table+48>:	0x080003e1	0x00000000	0x08002c11	0x08002c59
0x8000040 <g_vector_table+64>:	0x080003e1	0x080003e1	0x080003e1	0x080003e1
...                         (đến hết 59 word: toàn bộ IRQ ngoại vi = 0x080003e1)
+p/x &Default_Handler
$28 = 0x80003e0
```

| Kiểm tra | Giá trị đọc | Diễn giải |
|---|---|---|
| ICSR.VECTACTIVE | `0` | Thread mode, không có ISR active |
| NVIC ISER0, ISER1 | `0x00000000`, `0x00000000` | Không IRQ ngoại vi nào (IRQ 0..42 của STM32F103) được bật |
| NVIC IP[0..59] | Toàn `0x00` | Giá trị reset; không có ý nghĩa vì không IRQ nào được bật |
| SHPR3 (`0xE000ED20`) | `0xf0f00000` | PendSV = `0xF0`, SysTick = `0xF0` (priority thấp nhất) |
| Ngưỡng syscall `ucMaxSysCallPriority` | `0x50` | Ngắt gọi API kernel phải có giá trị ≥ `0x50` |
| So sánh | `0xF0 ≥ 0x50` | SysTick và PendSV hợp lệ |
| AIRCR.PRIGROUP | `(0xfa050300 >> 8) & 7 = 3`; khớp `ulMaxPRIGROUPValue = 0x300` | 4 bit preemption, đúng yêu cầu port CM3 |
| Vector table | Chỉ SVC `0x08002a01`, PendSV `0x08002c11`, SysTick `0x08002c59` khác `Default_Handler` (`0x080003e1`) | Không có ISR ngoại vi nào trong firmware |
| `info functions FromISR` | Rỗng | Không có API `...FromISR` nào được link vào firmware |
| CFSR, HFSR | `0x00000000`, `0x00000000` | Không có fault (assert sai priority trên CM3 thường kết thúc ở fault hoặc vòng `configASSERT`) |
| BASEPRI, PRIMASK | `0x0`, `0x0` | Không bị kẹt trong critical section |

### 2.8. [S1] / [S2] Hai lần halt cách nhau 2000 ms

| Biểu thức | Lần 1 | Lần 2 | Thay đổi |
|---|---|---|---|
| `xTickCount` | `2033` | `4049` | `+2016` tick (đúng ≈ 2000 ms ở 1 kHz cộng thời gian GDB đọc) |
| `$pc` | `0x8001cf0` | `0x8001c48` | Đều trong Idle (`prvCheckTasksWaitingTermination` / `prvIdleTask`) |
| `pxCurrentTCB->pcTaskName` | `"IDLE"` | `"IDLE"` | Không đổi |
| `s_task_a_tcb.pxTopOfStack` | `0x200008a0` | `0x200008a0` | Không đổi: `task_a` không được chạy lần nào |
| `s_task_b_tcb.pxTopOfStack` | `0x20000640` | `0x20000640` | Không đổi: `task_b` không được chạy lần nào |
| `s_mutex1` holder / số waiter | `0x2000091c` / `1` | `0x2000091c` / `1` | Không đổi |
| `s_mutex2` holder / số waiter | `0x200006c0` / `1` | `0x200006c0` / `1` | Không đổi |
| `xSuspendedTaskList.uxNumberOfItems` | `3` | `3` | Không đổi |

Hệ thống không treo cứng (SysTick vẫn chạy, Idle vẫn chạy), nhưng hai task ứng dụng đã đứng yên từ tick 100 và tick 120 đến ít nhất tick 4049. Bắt ở hai thời điểm halt khác nhau cho cùng một trạng thái, nên kết luận không phụ thuộc vào một thời điểm halt đơn lẻ.

## 3. Bảng địa chỉ symbol (từ `nm -S -n w3e4.elf`) và trạng thái task

### 3.1. Symbol trong SRAM

| Symbol | Địa chỉ | Size (byte) | Có trong cửa sổ `0x20003300..0x20003AFF`? |
|---|---:|---:|---|
| `s_timer_stack` / `s_timer_tcb` | `0x20000008` / `0x20000208` | 512 / 92 | Không |
| `s_idle_stack` / `s_idle_tcb` | `0x20000264` / `0x20000464` | 512 / 92 | Không |
| `s_task_b_stack` / `s_task_b_tcb` | `0x200004C0` / `0x200006C0` | 512 / 92 | Không |
| `s_task_a_stack` / `s_task_a_tcb` | `0x2000071C` / `0x2000091C` | 512 / 92 | Không |
| `s_mutex2` / `s_mutex1` (handle) | `0x20000978` / `0x2000097C` | 4 / 4 | Không |
| `s_mutex2_storage` / `s_mutex1_storage` | `0x20000980` / `0x200009D0` | 80 / 80 | Không |
| `pxCurrentTCB` | `0x20000A60` | 4 | Không |
| `pxReadyTasksLists` | `0x20000A64` | 160 | Không |
| `xDelayedTaskList1` / `xDelayedTaskList2` | `0x20000B04` / `0x20000B18` | 20 / 20 | Không |
| `pxDelayedTaskList` / `pxOverflowDelayedTaskList` | `0x20000B2C` / `0x20000B30` | 4 / 4 | Không |
| `xPendingReadyList` | `0x20000B34` | 20 | Không |
| `xSuspendedTaskList` | `0x20000B60` | 20 | Không |
| `uxCurrentNumberOfTasks` / `xTickCount` | `0x20000B74` / `0x20000B78` | 4 / 4 | Không |
| `ucMaxSysCallPriority` | `0x20001C80` | 1 | Không |
| `_ebss` | `0x20001C88` | — | Không (hết `.bss`) |

Giá trị đọc trên board khớp từng địa chỉ trong bảng: `p/x pxCurrentTCB = 0x20000464 = s_idle_tcb`, `p/x s_mutex1 = 0x200009d0 = s_mutex1_storage`, `p/x s_mutex2 = 0x20000980 = s_mutex2_storage`, `pxDelayedTaskList = 0x20000b04 = xDelayedTaskList1`, `pxOverflowDelayedTaskList = 0x20000b18 = xDelayedTaskList2`.

### 3.2. Trạng thái toàn bộ task tại halt lần 1 (tick 2033)

| Task | TCB | Priority (cur / base) | Trạng thái | State list | `pxStack` | `pxTopOfStack` | Đang chờ | Giữ mutex |
|---|---|---|---|---|---|---|---|---|
| `IDLE` | `0x20000464` | 0 / 0 | **Running** (`pxCurrentTCB`) | `pxReadyTasksLists[0]` | `0x20000264` | `0x20000410` (PSP thật `0x20000450`) | — | 0 |
| `task_a` | `0x2000091C` | 1 / 1 | **Blocked vô hạn** | `xSuspendedTaskList` | `0x2000071C` | `0x200008A0` | `s_mutex2` (event container `0x200009A4`) | 1 (`s_mutex1`) |
| `task_b` | `0x200006C0` | 1 / 1 | **Blocked vô hạn** | `xSuspendedTaskList` | `0x200004C0` | `0x20000640` | `s_mutex1` (event container `0x200009F4`) | 1 (`s_mutex2`) |
| `Tmr Svc` | `0x20000208` | 7 / 7 | Blocked vô hạn (bình thường) | `xSuspendedTaskList` | `0x20000008` | `0x20000190` | `xStaticTimerQueue.0` (event container `0x20000C40`) | 0 |

## 4. Giải thích sai lệch (ĐỊNH LƯỢNG so với PREDICTION.md)

### 4.1. Dự đoán từ symbol table so với kết luận sau khi đọc RAM

Mọi giá trị dự đoán ở `PREDICTION.md` mục 6.5 đều khớp số đo:

| Biểu thức | Dự đoán (mục 6.5) | Đo được | Lệch |
|---|---|---|---|
| `pxCurrentTCB` | `0x20000464` (`IDLE`) | `0x20000464` | 0 |
| `s_mutex1` holder | `0x2000091C` | `0x2000091c` | 0 |
| `s_mutex2` holder | `0x200006C0` | `0x200006c0` | 0 |
| `uxMessagesWaiting` hai mutex | `0`, `0` | `0`, `0` | 0 |
| Node wait list `s_mutex1` / `s_mutex2` | `0x200006D8` / `0x20000934` | `0x200006d8` / `0x20000934` | 0 |
| `task_a` / `task_b` event container | `0x200009A4` / `0x200009F4` | `0x200009a4` / `0x200009f4` | 0 |
| `uxMutexesHeld` `task_a` / `task_b` | `1` / `1` | `1` / `1` | 0 |
| `xSuspendedTaskList.uxNumberOfItems` | `3` | `3` | 0 |
| Delayed / overflow list | Rỗng | Rỗng | 0 |
| Tick `task_a` / `task_b` block | 100 / 120 | `xItemValue = 0x64 = 100` / `0x78 = 120` | 0 |
| 4 word đáy stack cả 4 task | `0xA5A5A5A5` | 16/16 word `0xA5A5A5A5` ở cả 4 task | 0 |
| NVIC ISER0..1 | `0`, `0` | `0`, `0` | 0 |
| Cửa sổ PARAMS | Không có object kernel | 0/512 word là pointer vào `.bss` | 0 |

Ở Chốt 1 (trước khi có ELF), không có dự đoán về loại lỗi. Ở Chốt 2, phân tích tĩnh xếp deadlock cao nhất nhưng ghi rõ là chưa đủ để kết luận. Symbol table và mã máy chỉ cho thấy **đường đi có thể** dẫn tới deadlock (hai thứ tự khóa ngược nhau). Chúng không chứng minh được:

- vòng chờ thật sự hình thành trên board (thứ tự đan xen phụ thuộc thời gian, `vTaskDelay(20)` của `task_b` là thứ quyết định);
- không có timeout hay task khác nhả khóa;
- stack không bị ghi đè trong lúc chạy (chỉ đọc được qua watermark trên RAM thật);
- NVIC không bị cấu hình từ đâu khác (chỉ đọc được qua thanh ghi lúc chạy).

Phần chứng minh chỉ có được từ RAM: holder thật, wait list thật, hai lần halt không đổi.

Ngoài ra, phân tích tĩnh không dự đoán được hai giá trị phụ: event item `xItemValue = 7` (`configMAX_PRIORITIES − uxPriority = 8 − 1`) và priority SysTick/PendSV = `0xF0`. Cả hai đều hợp lệ.

### 4.2. Số lệnh GDB thực tế so với dự đoán

| Mốc | Số lệnh |
|---|---:|
| Dự đoán Chốt 1 | 18..30 |
| Dự đoán Chốt 2 (sau khi biết 8 priority, 4 task) | ≈ 60 |
| Thực tế trong log (không tính `set`, `if`, `echo`, `eval`, helper) | 84 |
| Lệnh khác nhau dùng làm bằng chứng trong mục 1 | 14 |
| Ngưỡng `W3E4_MIN_GDB_EVIDENCE_COMMANDS` | 6 |

Số thực tế cao hơn dự đoán Chốt 2 vì script đọc thêm toàn bộ NVIC IP (60 byte), vector table (59 word) và chụp lần halt thứ hai để chứng minh không tiến triển.

## 5. Nguyên nhân gốc

- **Kết luận: DEADLOCK** giữa `task_a` và `task_b` do lấy hai mutex theo thứ tự ngược nhau. `task_a` lấy `s_mutex1` rồi chờ `s_mutex2`. `task_b` lấy `s_mutex2` rồi chờ `s_mutex1`. Cả hai chờ với `portMAX_DELAY`.

- **Bằng chứng XÁC NHẬN** (số # ở bảng mục 1):
  - **#8, #9**: `s_mutex1.xMutexHolder = 0x2000091c` (`task_a`), `s_mutex2.xMutexHolder = 0x200006c0` (`task_b`). Cả hai `uxMessagesWaiting = 0`. Wait list `s_mutex1` chứa `task_b + 24`, wait list `s_mutex2` chứa `task_a + 24`. Đây là chu trình đóng `task_a → s_mutex2 → task_b → s_mutex1 → task_a`.
  - **#8, #9 (phía TCB, mục 2.5)**: `task_a.xEventListItem.pvContainer = 0x200009a4 = s_mutex2_storage + 36`, `task_b.xEventListItem.pvContainer = 0x200009f4 = s_mutex1_storage + 36`, `uxMutexesHeld = 1` ở cả hai. Liên kết hai chiều khớp.
  - **#7**: cả hai task ở `xSuspendedTaskList`, delayed list rỗng. Đây là chờ vô hạn, không có timeout phá vòng. Wake tick cuối là 100 và 120, khớp thời điểm gọi `Take` lần hai.
  - **#4, #6**: chỉ `IDLE` ở Ready và đang chạy.
  - **S**: giữa tick 2033 và 4049, holder, waiter và `pxTopOfStack` của cả hai task không đổi. Hai task không tiến triển trong 2016 tick.

- **Bằng chứng LOẠI TRỪ 2 khả năng còn lại**:
  - **Stack overflow bị loại**:
    - **#10**: cả 4 task (kể cả `IDLE` và `Tmr Svc`) còn 16/16 word `0xA5A5A5A5` ở đáy. Watermark còn 332..440 byte trên 512 byte, dùng tối đa 35%.
    - **#10, #2**: mọi `pxTopOfStack` và PSP lúc halt (`0x20000450`) nằm trong vùng cấp phát của task.
    - **#2, #14**: CPU không ở HardFault hay `vApplicationStackOverflowHook` (PC trong Idle, `control = 0x2`, CFSR = HFSR = 0). Hardware breakpoint tại hook không bị chạm trong 4049 tick, dù ELF bật `configCHECK_FOR_STACK_OVERFLOW = 2`.
    - **#8, #9**: các list kernel liên kết đúng (`visited == declared`, owner và container khớp). Không có dấu hiệu ghi đè RAM.
  - **Sai priority ISR bị loại**:
    - **#12**: NVIC ISER0..1 = 0. Không có IRQ ngoại vi nào được bật, nên không có ISR ngoại vi nào có thể chạy.
    - **#11**: ICSR.VECTACTIVE = 0. Không ISR nào active lúc halt.
    - **#13**: hai ngắt duy nhất có tương tác với kernel là SysTick và PendSV, priority `0xF0` ≥ ngưỡng `ucMaxSysCallPriority = 0x50`. Hợp lệ. PRIGROUP = 3 khớp `ulMaxPRIGROUPValue = 0x300`.
    - **Mục 2.7**: vector table chỉ có SVC, PendSV, SysTick khác `Default_Handler`. `info functions FromISR` rỗng. Firmware không có đường ISR → API kernel nào có thể vi phạm ngưỡng.
    - **#14**: CFSR = HFSR = 0, BASEPRI = 0. Không có fault hay assert do sai priority.

- **Cách sửa (để tham khảo, không sửa ELF)**: cho cả hai task lấy khóa cùng một thứ tự (ví dụ luôn `s_mutex1` trước `s_mutex2`). Cách khác là dùng timeout hữu hạn khi `Take` lần hai, nếu hết hạn thì nhả khóa đầu rồi thử lại.
