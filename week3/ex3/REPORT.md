# W3.3 – REPORT

## Cách đo

- Nạp `cmake --build build/w3e3 --target w3e3_flash`, bắt log bằng
  `py tools/w3e3_capture.py --port COM19 --elf build/w3e3/w3e3.elf --log w3e3_run.log` (USART1 115200 8N1).
  Script lưu log và tự chạy `arm-none-eabi-addr2line -f -p -a <PC> <(LR & ~1) − 2>` cho mỗi bản ghi.
- Firmware tự chạy lần lượt 4 fault, mỗi fault một lần reset: ghi canary + bước vào `.noinit` → gây lỗi → handler
  ghi bản ghi → `NVIC_SystemReset()` → `main()` in bản ghi → bước kế tiếp. Chuỗi dừng ở `W3E3 DONE`.
- Log gốc: [measurements/w3e3_run2_null_guard_on.log](measurements/w3e3_run2_null_guard_on.log) (lần đạt cả 4 fault)
  và [measurements/w3e3_run1_null_guard_off.log](measurements/w3e3_run1_null_guard_off.log) (lần bộ canh NULL bị tắt).
- Địa chỉ dưới đây là của bản build lúc chạy lần 2. Bản build lúc viết PREDICTION khác địa chỉ (thêm code chẩn đoán vào
  `main.c` làm các hàm dịch đi), nhưng số dòng nguồn không đổi.

## Lựa chọn sau khi bắt fault: reset có kiểm soát (không treo)

- Handler ghi bản ghi `.noinit`, chờ có giới hạn cho USART1 phát xong (`TC`), `__DSB()` rồi `NVIC_SystemReset()`.
- Lý do chọn reset thay vì vòng lặp vô hạn:
  + Ngoài phòng thí nghiệm không có debugger để đọc trạng thái lúc treo. Reset đưa thiết bị về trạng thái chạy được, và bản
    ghi sống sót qua reset mềm nên vẫn in đầy đủ ở lần boot sau (đây chính là mục đích của `.noinit`).
  + Trong handler không in log: UART polling trong ngữ cảnh lỗi có thể chạm vào trạng thái đang hỏng. In ở `main()` sau reset
    thì chạy trong ngữ cảnh sạch.
  + Cho phép chạy cả 4 fault liên tiếp mỗi fault một lần reset riêng mà không cần thao tác tay.
- Chống vòng lặp reset vô tận: mỗi bước chỉ chạy một lần (bước kế = `fault_step + 1`). Fault ngoài kịch bản (chưa ghi canary)
  vẫn được ghi nhưng in `MISMATCH` và **dừng chuỗi** (`W3E3 HALT`). Bản ghi có checksum nên SRAM rác lúc cấp nguồn không bị
  nhận nhầm là bản ghi thật.
- Đánh đổi: nếu cùng một lỗi xảy ra mỗi lần khởi động **trước** `main()`, reset sẽ lặp lại liên tục. Bài này không gặp
  trường hợp đó. Sản phẩm thật nên thêm watchdog hoặc bộ đếm số lần reset liên tiếp.

## Số đo

Với MỖI loại fault trong `W3E3_ASSIGNED_FAULT_NAMES` (lần chạy 2, mọi fault xảy ra trong task `FaultGen`,
`exc_return = 0xFFFFFFFD` = Thread mode + PSP, `sp_valid = 1`):

| Loại fault | CFSR (hex) | HFSR (hex) | BFAR/MMFAR (hex, valid?) | PC stacked | Dòng code (`addr2line`) |
|---|---|---|---|---|---|
| NULL_DEREF_WRITE | `0x00000400` IMPRECISERR (DFSR `0x4` DWTTRAP) | `0x40000000` FORCED | BFAR `0x00000000` **không hợp lệ**, MMFAR không hợp lệ | `0x08000FEE` | PC → `fault_inject.c:26` (dấu `}`). Lệnh `str` gây lỗi ở `PC − 4 = 0x08000FEA` → **`fault_inject.c:25`** `*target = 0xDEADBEEFu;` |
| NULL_DEREF_READ | `0x00000000` (nguồn: DebugMonitor, DFSR `0x00000004` DWTTRAP) | `0x00000000` | Cả hai không hợp lệ | `0x08001008` | PC → **`fault_inject.c:33`** `s_sink = *source;` (lệnh `ldr` ở `0x08001006`, PC là lệnh kế cùng dòng) |
| INVALID_PC_JUMP | `0x00020000` INVSTATE | `0x40000000` FORCED | Cả hai không hợp lệ | `0x08000FC8` (đích nhảy, xPSR = `0x00000000`, bit T = 0) | PC → `fault_jump_landing` dòng 17 (đích, không phải chỗ lỗi). LR `0x0800102B` → `0x08001028` → **`fault_inject.c:42`** `jump();` |
| UNALIGNED_WORD_ACCESS | `0x01000000` UNALIGNED | `0x40000000` FORCED | Cả hai không hợp lệ, địa chỉ lệch nằm trong stacked `r2 = 0x200006A9` | `0x0800104E` | PC → **`fault_inject.c:51`** `s_sink = *(const volatile uint32_t *)address;` |

- Canary đọc lại sau mỗi lần reset: **khớp ở cả 4 lần** (`canary=0x6C217697 MATCH`, `reset=SOFT`). Cả lần chạy 1 cũng khớp ở
  3 lần reset do fault. Lần boot đầu mỗi chuỗi in `PM none` đúng như dự đoán (chưa có bản ghi).
- Bản ghi `fault_record_t`: 96 byte tại `0x20000770` trong `.noinit` (NOLOAD, sau `_ebss`), có checksum FNV-1a.

## Giải thích sai lệch (ĐỊNH LƯỢNG)

- **INVALID_PC_JUMP, UNALIGNED_WORD_ACCESS: khớp dự đoán hoàn toàn.** CFSR đúng bit (`INVSTATE` / `UNALIGNED`),
  HFSR `FORCED`, BFAR/MMFAR không hợp lệ, dòng 42 / dòng 51 đúng như PREDICTION. Riêng INVALID_PC_JUMP, PC stacked là
  địa chỉ đích (lệnh đầu tiên CPU cố chạy khi bit T = 0) nên dòng lỗi phải lấy từ LR, cũng đúng như đã dự đoán.
- **NULL_DEREF_READ: khớp dự đoán.** Đọc `0x0` không gây lỗi bus vì `0x0` là bí danh Flash: stacked `r2 = 0x20005000`
  chính là MSP ban đầu ở ô đầu bảng vector, chứng tỏ lệnh đọc thật sự đã đọc được dữ liệu. Chỉ bộ canh DWT bắt được
  (CFSR = HFSR = 0, DFSR = DWTTRAP). Sự kiện DWT không chính xác: PC là lệnh **kế tiếp** (`0x08001008`) sau lệnh `ldr`
  (`0x08001006`), nhưng cả hai cùng thuộc dòng 33 nên addr2line vẫn đúng dòng.
- **NULL_DEREF_WRITE: lệch dự đoán.** PREDICTION đặt khả năng chính là `PRECISERR + BFARVALID` (`0x00008200`) với
  BFAR = `0x0`. Thực tế là **`IMPRECISERR` (`0x00000400`), BFAR không hợp lệ**:
  + Ghi vào bí danh Flash khi chưa bật `FLASH_CR.PG` đúng là bị trả lỗi bus, nhưng lỗi đến **trễ** (bất đồng bộ).
    `ACTLR.DISDEFWBUF` không giúp được vì bit này chỉ tắt bộ đệm ghi cho vùng nhớ mặc định không được ánh xạ, còn
    `0x0` là vùng Flash đã ánh xạ.
  + Hệ quả đo được: ở lần chạy 1 (không có DWT), PC stacked là `0x08001042`, nằm trong `fault_inject_by_name` **sau khi**
    `inject_null_write` đã chạy hết `mov`, `pop`, `bx lr`. addr2line trên PC ra `fault_inject.c:73`, **sai dòng**.
  + Ở lần chạy 2 (có DWT canh ghi), sự kiện DWT buộc CPU vào ngoại lệ sớm hơn. Lúc vào ngoại lệ, bộ đệm ghi được xả
    và lỗi bus bất đồng bộ lộ ra, leo thang thành HardFault (ưu tiên −1, cao hơn DebugMonitor). Vì vậy log có cả
    `IMPRECISERR` lẫn `DWTTRAP`, và PC stacked chỉ trễ 2 lệnh (`0x08000FEE` so với `str` ở `0x08000FEA`).
  + Dòng lỗi vẫn xác định được chính xác: stacked `r2 = 0xDEADBEEF` là giá trị đang ghi, `step=0` cho biết hàm gây
    lỗi là `inject_null_write`, và lùi từ PC về lệnh `str` gần nhất → `0x08000FEA` → **`fault_inject.c:25`**.
- **Lần chạy 1: `NULL_DEREF_READ` ra `NO_FAULT`.** Dòng chẩn đoán in `c_debugen=1 numcomp=4`: chip có đủ 4 bộ so sánh
  DWT, nhưng OpenOCD để lại `DHCSR.C_DEBUGEN = 1` sau khi nạp. Bit này phần mềm không xóa được và chỉ mất khi cấp
  nguồn lại (log toàn `reset=PIN`, không có `POR`, nghĩa là board chưa mất nguồn). Khi bit này bật, sự kiện DWT dừng CPU
  thay vì vào DebugMonitor, nên firmware chủ động không bật bộ canh. Cách xử lý: xóa bit qua SWD
  (`mww 0xE000EDF0 0xA05F0000`) rồi reset; target `w3e3_flash` nay tự làm hai bước này sau khi nạp.

## Trả lời câu hỏi trade-off

Với loại fault mà BFAR/MMFAR không hợp lệ (`BFARVALID`/`MMARVALID` = 0), bạn còn cách nào khác để xác định địa
chỉ gây lỗi ngoài CFSR/HFSR? Chứng minh bằng chính log của bạn.

- **Thanh ghi trong stacked frame.** Lệnh gây lỗi thường còn giữ địa chỉ trong thanh ghi nguồn. Ở UNALIGNED_WORD_ACCESS,
  `ldr r2, [r2]` có `r2 = 0x200006A9` trong stacked frame: đó chính là địa chỉ lệch, dù BFAR/MMFAR không hợp lệ (UsageFault
  không bao giờ ghi hai thanh ghi này). Ở NULL_DEREF_WRITE, `str r2, [r3]` có `r3 = 0x00000000` (địa chỉ NULL) và
  `r2 = 0xDEADBEEF` (giá trị đang ghi).
- **Disassembly quanh PC.** Với lỗi bất đồng bộ (`IMPRECISERR`), PC chỉ là chỗ lỗi được báo về. Lùi lại tìm lệnh ghi bộ nhớ
  gần nhất: lần chạy 2 tìm được `str` ở `0x08000FEA` cách PC 2 lệnh, lần chạy 1 phải lùi qua cả lệnh trả về của hàm.
- **LR.** Khi PC đã nhảy tới địa chỉ sai (INVALID_PC_JUMP, PC = `0x08000FC8`), LR `0x0800102B` vẫn chỉ ra lệnh `blx` đã gọi
  tới đó (`fault_inject.c:42`).
- **Ngữ cảnh của firmware.** Bản ghi lưu `fault_step` và tên task, nên biết ngay hàm nào đang chạy lúc lỗi (`FaultGen`,
  `NULL_DEREF_WRITE` → `inject_null_write`).
- **Đổi lỗi bất đồng bộ thành đồng bộ.** `ACTLR.DISDEFWBUF` biến BusFault ghi thành chính xác với vùng nhớ mặc định không
  được ánh xạ, nhưng log cho thấy nó **không** có tác dụng với lỗi ghi vào Flash (vẫn `IMPRECISERR` ở cả hai lần). Một bộ
  canh DWT trên vùng địa chỉ nghi ngờ thì có tác dụng thật: nó kéo PC stacked từ `0x08001042` (sai hàm) về `0x08000FEE`
  (đúng hàm, trễ 2 lệnh).
