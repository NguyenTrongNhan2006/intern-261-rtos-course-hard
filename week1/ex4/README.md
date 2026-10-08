# W1.4 – 100% cấp phát tĩnh + ngân sách RAM

## Mục tiêu
Quan sát phân mảnh heap_4 bằng chính chuỗi cấp phát/giải phóng của bạn, rồi chuyển toàn hệ thống sang cấp phát tĩnh
100% và đối chiếu ngân sách RAM với file `.map`.

## Đọc trước
- Mastering chương 3 (quản lý bộ nhớ, heap_1..5, `*Static` API)
- Tài liệu GNU ld: đọc output `.map` (section sizes, symbol table)

## Việc phải làm
1. Với `heap_4` (giữ nguyên `configSUPPORT_DYNAMIC_ALLOCATION=1` cho bước này): cấp phát `W1E4_NUM_BLOCKS` khối theo
   `W1E4_BLOCK_SIZE_BYTES` (xem `params.h`), rồi giải phóng theo đúng thứ tự `W1E4_FREE_ORDER`, lặp lại
   `W1E4_REPEAT_CYCLES` lần. Ghi `xPortGetFreeHeapSize()` và `xPortGetMinimumEverFreeHeapSize()` sau mỗi bước.
2. Phân tích log: phân mảnh có xảy ra không, vì sao (dựa trên thuật toán coalescing theo địa chỉ của heap_4).
3. Chuyển toàn bộ hệ thống (mọi task/queue/mutex đã tạo ở W1.1–W1.3) sang `configSUPPORT_DYNAMIC_ALLOCATION=0`,
   dùng `xTaskCreateStatic`, `xQueueCreateStatic`, `xSemaphoreCreateMutexStatic`, và cung cấp
   `vApplicationGetIdleTaskMemory` / `vApplicationGetTimerTaskMemory`.
4. Dùng `uxTaskGetStackHighWaterMark()` để đo biên an toàn stack mỗi task, rồi tinh chỉnh kích thước stack khai báo tĩnh.
5. Tính tay bảng ngân sách RAM (.data + .bss + mọi buffer tĩnh + stack từng task) và so với file `.map` thật
   (sai số cho phép: `W1E4_RAM_BUDGET_TOLERANCE_BYTES`, xem `PARAMS.md`).

## Sản phẩm nộp
- Log phân mảnh heap_4 (toàn bộ `W1E4_REPEAT_CYCLES` vòng).
- Hệ thống chạy ổn định với 0 byte cấp phát động (`check_api.sh` xác nhận không còn `pvPortMalloc`/`xTaskCreate` v.v. từ bài này trở đi).
- Bảng RAM tự tính khớp `.map` trong sai số cho phép.

## Tiêu chí đạt
Giải thích được vì sao phân mảnh xảy ra (hay không xảy ra) với đúng chuỗi cấp phát/giải phóng của chính mình.

## Giờ dự kiến
3 giờ.
