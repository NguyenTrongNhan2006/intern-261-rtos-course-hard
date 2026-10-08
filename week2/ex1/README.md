# W2.1 – Driver I2C ngắt + phục hồi bus

## Mục tiêu
Tự viết driver I2C1 mức thanh ghi, interrupt-driven, báo hoàn tất bằng task notification, có timeout và bus
recovery (toggle SCL) theo errata F103, dùng chung an toàn giữa 2 task client qua mutex.

## Đọc trước
- RM0008 chương 26 (I2C) – đọc kỹ phần trình tự ngắt sự kiện EV5–EV8 (master transmit/receive).
- Errata sheet STM32F103 ("I2C limitations" – các trường hợp bus lock khi SDA bị kéo xuống).
- Datasheet MPU6050 (register map, I2C address 0x68/0x69, timing).
- Mastering ch.6 (task notification) + freertos.org *Task Notifications*.

## Việc phải làm
1. Driver I2C1 thanh ghi (CR1/CR2/SR1/SR2/DR/CCR/TRISE), interrupt-driven: bật `ITEVFEN`/`ITERREN`/`ITBUFEN`,
   cấu hình NVIC cho `I2C1_EV_IRQn`/`I2C1_ER_IRQn` dưới `configMAX_SYSCALL_INTERRUPT_PRIORITY`.
2. ISR gọi `xTaskNotifyFromISR` báo hoàn tất transaction (hoặc lỗi) cho task đang chờ bằng `ulTaskNotifyTake`.
3. Timeout mềm (`W2E1_I2C_TIMEOUT_MS` trong `params.h`) khi không có phản hồi ISR trong thời gian cho phép.
4. Bus recovery: khi phát hiện SDA bị kéo xuống dai dẳng (bus busy không giải phóng), tạm chuyển SCL/SDA sang
   GPIO thường, toggle SCL thủ công tối đa 9 xung để thiết bị slave nhả SDA, rồi phát START/STOP để giải phóng bus,
   cuối cùng trả I2C1 về chế độ AF.
5. 2 task client (ví dụ: đọc accel, đọc gyro từ MPU6050) dùng chung bus qua 1 mutex static
   (`xSemaphoreCreateMutexStatic`).
6. Test rút dây SDA giữa transaction (thủ công hoặc qua relay/GPIO mô phỏng): hệ thống phải tự phục hồi, không
   reset MCU, lặp lại được `W2E1_RECOVERY_REPEAT_COUNT` lần liên tiếp (`params.h`).

## Sản phẩm nộp
- Driver I2C1 hoàn chỉnh + 2 task client qua mutex.
- Ảnh logic analyzer: trình tự ngắt EV5–EV8 bình thường, và trình tự bus recovery khi rút dây SDA.
- Log đếm số lần phục hồi thành công liên tiếp.

## Tiêu chí đạt
Phục hồi được ≥ `W2E1_RECOVERY_REPEAT_COUNT` lần liên tiếp (xem `PARAMS.md`, tối thiểu 10 theo đề); giải thích đúng
trình tự EV5–EV8 trên ảnh logic analyzer của chính mình.

## Giờ dự kiến
5 giờ.
