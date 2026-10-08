# W4.4 – Bảo vệ

## Mục tiêu
Buổi bảo vệ 15 phút (`W4E4_DEFENSE_DURATION_MIN`): mentor đổi thông số tại chỗ (trong nhóm tham số được gợi ý ở
`W4E4_MUTATION_TARGET_HINT`, giá trị cụ thể KHÔNG được tiết lộ trước) VÀ tiêm `W4E4_UNKNOWN_FAULT_COUNT` lỗi
sinh viên chưa biết trước. Đây là bài tổng hợp toàn bộ tuần 4 (và gián tiếp cả khóa), không có code mới.

## Đọc trước
- Toàn bộ design doc (W4.1), code đã nộp (W4.2), log kiểm chứng + fault injection (W4.3) của chính bạn.
- Ôn lại mọi câu hỏi "cờ đỏ" trong `oral.md` của W4.1/W4.2/W4.3 (mentor có thể hỏi lại bất kỳ câu nào).

## Việc phải làm
1. Chuẩn bị tóm tắt 1 trang: kiến trúc hệ thống, bảng RTA, các kịch bản fault đã kiểm chứng, số đo chính.
2. Trong buổi bảo vệ: khi mentor đổi 1 tham số thuộc nhóm `W4E4_MUTATION_TARGET_HINT`, PHẢI dự đoán HƯỚNG ảnh
   hưởng (tăng/giảm WCRT, jitter, RAM, hay gì khác) TRƯỚC khi cho build lại/chạy lại.
3. Khi mentor tiêm lỗi chưa biết trước: dùng chính công cụ đo đã xây (tracer, run-time stats, `.noinit`
   post-mortem, GDB) để chẩn đoán tại chỗ, không được đoán mò.
4. Trả lời các câu hỏi vấn đáp tổng hợp (mentor chọn từ `oral.md` của W4.1-W4.3 và có thể hỏi xuyên suốt cả
   khóa, ví dụ liên hệ lại W1.3/W2.3/W3.3).

## Sản phẩm nộp
- Không có code mới. Bản tóm tắt 1 trang (có thể là phần đầu REPORT.md) mang theo buổi bảo vệ.
- `include/defense_summary.h` khai báo cấu trúc tóm tắt số liệu mang theo buổi bảo vệ (điền thủ công, không
  phải code chạy trên MCU).

## Tiêu chí đạt
Dự đoán đúng HƯỚNG ảnh hưởng khi mentor đổi tham số tại chỗ; chẩn đoán được lỗi tại chỗ bằng công cụ đo của
chính mình trong thời lượng `W4E4_DEFENSE_DURATION_MIN`.

## Giờ dự kiến
1 giờ.
