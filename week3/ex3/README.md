# W3.3 – HardFault handler + post-mortem

## Mục tiêu
Viết handler (asm + C) tự lưu lại đầy đủ bối cảnh crash (stacked frame, CFSR/HFSR/BFAR/MMFAR, tên task) vào
vùng RAM `.noinit`, in lại sau reset, rồi dùng `addr2line` xác định đúng dòng code gây lỗi cho từng loại fault
được giao (xem `PARAMS.md`).

## Đọc trước
- PM0056 chương xử lý exception/fault (CFSR layout, HFSR, cách đọc stacked frame EXC_RETURN).
- RM0008 (bit field chi tiết SCB->CFSR/HFSR/MMFAR/BFAR).
- tài liệu `addr2line` (binutils) để tra PC → dòng code.

## Việc phải làm
1. Linker script của bạn (W1.1) đã có section `.noinit` – định nghĩa 1 struct post-mortem record (PC, LR, xPSR,
   CFSR, HFSR, BFAR, MMFAR, tên task 16 ký tự, canary) đặt vào đúng section đó.
2. `HardFault_Handler` (naked, asm) lấy đúng con trỏ stack đang active (MSP hay PSP, kiểm tra bit `EXC_RETURN`
   trong LR) rồi nhảy sang 1 hàm C truyền con trỏ stacked-frame vào.
3. Hàm C: đọc `SCB->CFSR/HFSR/MMFAR/BFAR`, `pcTaskGetName(xTaskGetCurrentTaskHandle())`, điền vào record
   `.noinit`, rồi halt (vòng lặp vô hạn hoặc reset có kiểm soát – tự quyết định, giải thích lựa chọn).
4. Sau reset: `main()` kiểm tra canary trong record (so với `W3E3_NOINIT_CANARY`) để biết có post-mortem hợp lệ
   hay không, nếu có thì in toàn bộ ra UART.
5. Lần lượt tự kích hoạt TỪNG loại fault trong `W3E3_ASSIGNED_FAULT_NAMES` (mỗi loại 1 lần chạy riêng, ghi
   canary trước khi gây fault), thu log, rồi `arm-none-eabi-addr2line -e <your.elf> <PC>` để chỉ ra đúng dòng
   code gây crash.

## Sản phẩm nộp
- Code HardFault handler + post-mortem `.noinit` + in log sau reset.
- Log crash cho từng loại fault trong `W3E3_ASSIGNED_FAULT_NAMES`.
- Với mỗi log: output `addr2line` chỉ đúng dòng code gây fault.

## Tiêu chí đạt
Tìm đúng dòng lỗi cho MỌI loại fault được giao bằng `addr2line`, log đầy đủ CFSR/HFSR/BFAR hoặc MMFAR phù hợp
từng loại, canary xác nhận `.noinit` hợp lệ.

## Giờ dự kiến
3 giờ.
