# W2.3 – Inversion, chained blocking, deadlock

## Mục tiêu
Tái hiện 4 task / 2 mutex: unbounded priority inversion (dùng binary semaphore, không inheritance), chained
blocking dù đã bật priority inheritance, và deadlock do thứ tự lock ngược nhau. Tự hiện thực wrapper Immediate
Priority Ceiling Protocol (IPCP) trên API gốc và so sánh thời gian block với inheritance.

## Đọc trước
- Mastering ch.8 (Mutex, priority inheritance).
- Một tài liệu chuẩn về Priority Ceiling Protocol (ví dụ Sha/Rajkumar/Lehoczky 1990, hoặc chương tương ứng trong
  giáo trình RTOS bất kỳ) – đọc phần Immediate Priority Ceiling cụ thể, không phải Basic/Original PCP.

## Việc phải làm
1. 4 task ưu tiên khác nhau (τ_H cao nhất, τ_M1, τ_M2, τ_L thấp nhất), 2 tài nguyên bảo vệ bởi 2 khóa riêng.
2. **Hiện tượng 1 – unbounded inversion**: dùng `xSemaphoreCreateBinaryStatic` (không inheritance) cho khóa 1;
   τ_L giữ khóa, τ_M (ưu tiên giữa τ_L và τ_H, KHÔNG đụng khóa) chạy chen vào, τ_H bị chặn gián tiếp vô thời hạn.
3. **Hiện tượng 2 – chained blocking**: dùng `xSemaphoreCreateMutexStatic` (có inheritance) nhưng để τ_H cần LẦN
   LƯỢT cả 2 khóa mà 2 task khác đang giữ theo thứ tự so le → inheritance không loại bỏ hết được việc τ_H bị chặn
   nhiều lần nối tiếp (chuỗi blocking dài hơn 1 lần).
4. **Hiện tượng 3 – deadlock**: 2 task lock 2 mutex theo thứ tự ngược nhau (A rồi B / B rồi A) → deadlock kinh
   điển; dùng tracer W1.2 (Gantt) để nhận diện đúng điểm cả hai dừng vĩnh viễn.
5. Tự viết wrapper `include/priority_ceiling.h` hiện thực Immediate Priority Ceiling trên API gốc
   (`vTaskPrioritySet`/`uxTaskPriorityGet` để nâng priority task lên ceiling lúc lock, trả về lúc unlock).
6. Lặp lại hiện tượng 1 và 2 dùng wrapper ceiling thay vì semaphore/mutex thường; đo lại thời gian τ_H bị block.
7. Thiết kế lại thứ tự lock (hoặc dùng ceiling) để CHỨNG MINH hiện tượng 3 (deadlock) không còn xảy ra.

## Sản phẩm nộp
- Gantt (từ tracer W1.2) cho CẢ BA hiện tượng + Gantt sau khi áp dụng ceiling.
- Bảng thời gian block của τ_H: binary semaphore (inversion) / mutex inheritance (chained blocking) / ceiling.
- Code wrapper priority ceiling.

## Tiêu chí đạt
Chỉ đúng điểm chained blocking trên Gantt (không nhầm với inversion thường); giải thích đúng vì sao ceiling chặn
được deadlock (không chỉ giảm thời gian block).

## Giờ dự kiến
4 giờ.
