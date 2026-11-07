# W3.3 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Bối cảnh dùng để dự đoán

- Tham số (`include/params.h`): `W3E3_ASSIGNED_FAULT_NAMES = {NULL_DEREF_WRITE, NULL_DEREF_READ, INVALID_PC_JUMP,
  UNALIGNED_WORD_ACCESS}`, `W3E3_NOINIT_CANARY = 0x6C217697`.
- Bản ghi `fault_record_t` (`include/fault_postmortem.h`): **96 byte**, đặt tại `0x20000770..0x200007CF` trong section
  `.noinit` (NOLOAD, nằm ngay sau `_ebss = 0x20000770`). Số liệu lấy từ `arm-none-eabi-objdump -h` và `nm -S` của
  `build/w3e3/w3e3.elf`.
- Mọi fault được gây trong task `FaultGen` (Thread mode, PSP), nên dự đoán **EXC_RETURN = `0xFFFFFFFD`** và stacked
  frame nằm trong stack của task (`s_fault_stack = 0x2000025C..0x2000065B`).
- Bẫy bật lúc khởi động: `CCR.UNALIGN_TRP = 1`, `CCR.DIV_0_TRP = 1`, `ACTLR.DISDEFWBUF = 1` (BusFault ghi trở thành
  chính xác). MemManage/BusFault/UsageFault không bật riêng trong `SHCSR`, nên mọi fault cấu hình được đều leo thang
  thành HardFault và dự đoán **HFSR = `0x40000000` (FORCED)**.
- STM32F103 **không có MPU**, và vùng `0x00000000` là bí danh của Flash khi boot từ Flash. Vì vậy truy cập NULL
  không tự động gây MemManage. Để bắt NULL, bộ so sánh DWT 0 canh `0x000..0x3FF` (đọc hoặc ghi) và sinh ngoại lệ
  DebugMonitor, đi chung đường ghi bản ghi với HardFault (`source=DebugMon(DWT)`). Bộ canh chỉ bật khi
  `DHCSR.C_DEBUGEN = 0` (không có debugger giữ halting debug).
- Địa chỉ dưới đây lấy từ `objdump -d` của bản build hiện tại. Đổi code sẽ đổi địa chỉ, còn dòng nguồn giữ nguyên.

## Dự đoán từng loại fault

| Loại fault | CFSR bit dự đoán sẽ set | BFAR/MMFAR có hợp lệ không | Dòng code dự đoán gây fault |
|---|---|---|---|
| NULL_DEREF_WRITE | Khả năng chính: `PRECISERR` + `BFARVALID` → **CFSR = `0x00008200`**, HFSR = `0x40000000`. Lần ghi word vào bí danh Flash khi chưa bật `FLASH_CR.PG` bị FPEC trả lỗi bus. Nếu FPEC bỏ qua lần ghi thay vì báo lỗi: không có BusFault, bộ canh DWT bắt thay, CFSR = `0`, DFSR = `0x00000004` (DWTTRAP) | **BFAR hợp lệ = `0x00000000`**, MMFAR không hợp lệ. Trong khả năng thứ hai thì cả hai đều không hợp lệ | `fault_inject.c:25` `*target = 0xDEADBEEFu;` (lệnh `str` ở `0x08000F9A`). PC stacked = `0x08000F9A`, LR = `0x08001043` (trong `fault_inject_by_name`). Với đường DWT, PC trỏ lệnh sau `0x08000F9C` → `fault_inject.c:26` |
| NULL_DEREF_READ | **Không có cờ CFSR nào** (CFSR = `0`, HFSR = `0`) vì đọc `0x0` trả về MSP ban đầu `0x20005000`, không phải lỗi bus. Chỉ bộ canh DWT bắt được: `source=DebugMon(DWT)`, **DFSR = `0x00000004` (DWTTRAP)**. Nếu `C_DEBUGEN = 1` thì bộ canh tắt và log in `NO_FAULT` | Cả hai **không hợp lệ** | `fault_inject.c:33` `s_sink = *source;` (lệnh `ldr` ở `0x08000FB6`). Sự kiện DWT không chính xác: PC stacked dự đoán là lệnh kế `0x08000FB8`, addr2line vẫn ra dòng 33 |
| INVALID_PC_JUMP | **`INVSTATE`** → **CFSR = `0x00020000`**, HFSR = `0x40000000`. xPSR stacked có bit T (bit 24) = 0 | Cả hai **không hợp lệ** (UsageFault không có thanh ghi địa chỉ) | PC stacked = đích nhảy `0x08000F78` (`fault_jump_landing`, `fault_inject.c:17`), **không phải dòng có lỗi**. Dòng lỗi lấy từ LR = `0x08000FDB`: addr2line `(LR & ~1) − 2 = 0x08000FD8` → `fault_inject.c:42` `jump();` |
| UNALIGNED_WORD_ACCESS | **`UNALIGNED`** → **CFSR = `0x01000000`**, HFSR = `0x40000000` | Cả hai **không hợp lệ**. Địa chỉ lệch `0x200006A9` vẫn đọc được từ stacked `r2` | `fault_inject.c:51` `s_sink = *(const volatile uint32_t *)address;` (lệnh `ldr r2, [r2]` ở `0x08000FFE`). PC stacked = `0x08000FFE`, LR = `0x08001043` |

## Canary

- Dự đoán canary `W3E3_NOINIT_CANARY` **đọc lại đúng sau mọi lần reset do fault**, vì handler reset bằng
  `NVIC_SystemReset()` (SYSRESETREQ). Reset này không làm mất nội dung SRAM. `Reset_Handler` chỉ chép `.data` và xóa
  `_sbss.._ebss`, còn `.noinit` nằm sau `_ebss` với thuộc tính NOLOAD nên không bị chạm. Log mong đợi `reset=SOFT` và
  `canary=0x6C217697 MATCH`.
- Canary **sẽ không khớp** (log `PM none`) khi cấp nguồn lại hoặc brown-out (SRAM ngẫu nhiên, checksum FNV-1a sai), và
  ở lần chạy đầu tiên sau khi nạp (SRAM còn dữ liệu của firmware cũ).
- Một fault ngoài kịch bản (chưa ghi canary) vẫn được ghi nhận nhưng in `canary=0x00000000 MISMATCH` và dừng chuỗi
  để tránh boot-loop.
