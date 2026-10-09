# W1.1 – PREDICTION (điền TRƯỚC khi chạy, commit trước)

## Dự đoán tần số

- Flash latency dự kiến ở 72 MHz (theo bảng wait-state RM0008 §4): ___ wait-state.
- Tần số xung TIM đo được dự đoán: 1000 Hz ± `W1E1_CLOCK_TOLERANCE_PPM` (xem PARAMS.md) = ___Hz đến___ Hz.

Phần Answer :

- Flash latency dự kiến ở 72 MHz: **2 wait-state**.

   Lý do: đề bài W1.1 yêu cầu chạy STM32F103 ở 72 MHz và đặt Flash latency 2 wait-state trước khi tăng PLL.  

- Tần số xung TIM dự đoán: **999.774 Hz đến 1000.226 Hz**.
  Lý do: tần số mục tiêu là 1000 Hz, sai số cho phép là 226 ppm.
  Sai số tuyệt đối = 1000 × 226 / 1,000,000 = **0.226 Hz**.
  Vì vậy sai số tương đối của tần số em dự đoán là 1000 ± 0.226 Hz.

## Dự đoán hành vi blinky

- Chu kỳ blink task A / B dự kiến (xem PARAMS.md): ___ms /___ ms.
- Ảnh hưởng của priority 2 task tới thứ tự chạy khi cùng ready: ___

Phần Answer :

- Khoảng thời gian giữa hai lần toggle GPIO của task A / B: **264 ms / 704 ms**.
  Lý do: đây là chu kỳ được tạo riêng theo MSSV trong `PARAMS.md`. 2452882
  Nếu mỗi lần task đổi trạng thái chân GPIO một lần, chu kỳ đầy đủ của dạng sóng sẽ là 528 ms với task A và 1408 ms với task B.

- Khi hai task cùng Ready: **task có priority cao hơn sẽ được scheduler chọn chạy trước. Nếu hai task có cùng priority, thứ tự phụ thuộc vào cấu hình lập lịch/time slicing; đề bài chưa quy định priority cụ thể cho hai task.**
  Lý do: FreeRTOS lập lịch theo priority. Với các task cùng priority, cơ chế time slicing và thứ tự trong danh sách Ready ảnh hưởng task nào chạy tại từng thời điểm. Cần ghi đúng theo priority và cấu hình thực tế mà mình chọn.

## Rủi ro đã lường trước

- Nếu quên set flash latency trước khi tăng PLL: dự đoán hệ quả ___
- Nếu PendSV priority không thấp nhất: dự đoán hệ quả ___

Phần Answer :

- Nếu quên đặt Flash latency trước khi tăng PLL: **CPU có thể không đọc Flash ổn định ở 72 MHz; chương trình có thể chạy sai, treo hoặc phát sinh lỗi.**
  Lý do: Flash cần thêm wait-state khi CPU chạy ở tần số cao. Phải cấu hình độ trễ Flash phù hợp trước khi chuyển clock lên 72 MHz.

- Nếu PendSV priority không thấp nhất: **PendSV có thể chạy chen vào lúc các interrupt ưu tiên cao hơn chưa xử lý xong, làm tăng độ trễ hoặc phá vỡ giả định về thứ tự xử lý của FreeRTOS.**
  Lý do: PendSV dùng để chuyển context task nên nên được trì hoãn đến khi các exception/interrupt quan trọng hơn hoàn tất.
