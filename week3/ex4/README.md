# W3.4 – Debug hộp đen (chỉ có .elf)

## Mục tiêu
Bạn được đưa `w3e4.elf` (có symbol, KHÔNG có source) chứa 1 trong các lỗi: deadlock, stack overflow, hoặc sai
priority ISR. Dùng OpenOCD + GDB đọc trực tiếp cấu trúc nội bộ FreeRTOS (TCB, ready/blocked list, stack) để tìm
ĐÚNG nguyên nhân gốc – không được đoán, mọi kết luận phải có bằng chứng GDB kèm theo.

## Đọc trước
- Mã nguồn `tasks.c` của FreeRTOS-Kernel (bản V11) – đọc kỹ định nghĩa `TCB_t`, `pxReadyTasksLists`,
  `pxDelayedTaskList`/`pxOverflowDelayedTaskList`, và cách `List_t`/`ListItem_t` tổ chức (danh sách vòng có
  phần tử "end marker", sắp theo `xItemValue`).
- Tài liệu GDB: `print`, `x` (examine memory), đọc struct qua con trỏ ép kiểu (`print *(TCB_t *)0x...`), cách
  tải symbol từ 1 `.elf` không có source (`file w3e4.elf`, `list` sẽ báo không tìm thấy source – đó là bình
  thường, không cần source để đọc được dữ liệu).

## Việc phải làm
1. Nạp `w3e4.elf` vào board qua OpenOCD, halt ngay (không `reset run` tự do nếu muốn bắt đúng trạng thái).
2. `nm w3e4.elf | grep -i -E "pxReadyTasksLists|pxCurrentTCB|xDelayedTaskList"` để tìm địa chỉ các biến kernel
   nội bộ liên quan (không hard-code địa chỉ, đọc trực tiếp từ `.elf` của chính bạn).
3. Quét vùng SRAM gợi ý trong `PARAMS.md` (`W3E4_RAM_SCAN_START`/`W3E4_RAM_SCAN_LEN`) bằng `x/...xw` để làm quen
   trước khi tra theo symbol, rồi đối chiếu với các symbol ở bước 2.
4. Duyệt `pxReadyTasksLists[i]` cho từng priority, và danh sách Blocked (delayed/suspended tuỳ trạng thái), ghi
   lại: task nào đang Ready, task nào đang Blocked và (nếu Blocked) đang chờ cái gì.
5. Với mỗi giả thuyết (deadlock / stack overflow / sai priority): tìm ít nhất 1 bằng chứng GDB XÁC NHẬN và ít
   nhất 1 bằng chứng cho thấy 2 giả thuyết còn lại SAI – đạt tối thiểu `W3E4_MIN_GDB_EVIDENCE_COMMANDS` lệnh GDB
   khác nhau có bằng chứng trong báo cáo.

## Sản phẩm nộp
- REPORT.md: nguyên nhân gốc kết luận + TOÀN BỘ lệnh GDB đã dùng kèm output làm bằng chứng (không tóm tắt,
  copy nguyên văn output quan trọng).
- Giải thích vì sao 2 giả thuyết còn lại bị loại.

## Tiêu chí đạt
Chỉ đúng nguyên nhân gốc, có bằng chứng từ GDB (không phải suy đoán từ hành vi bên ngoài như LED nhấp nháy).

## Giờ dự kiến
3 giờ.
