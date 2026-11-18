question week 1

1. Trong vector table ở file start up c , những vector nào được FreeRTOS sử dụng ?

2. cơ chế của switch in và switch out trong freeRTOS ?

3. chức năng của tcb ?

question week 2

Nhân :
 2. done : nếu head và tail , hoặc capacity tràn trong task 3 thì code em xử lý ra sao ? capacity ring buffer phải chọn thông số như nào ?

 1. task 4 : Nếu 1 task đến muộn hơn timeout, các task kia timeout. Bit của các task đã đến còn set hay bị xóa? Vòng barrier kế tiếp có bị sớm sai không? Code em xử lý thế nào?

bạn Tài

1. Timeout hết hạn, task coi transaction là lỗi nhưng ngay sau đó ISR mới tới và gọi xTaskNotifyFromISR. Transaction kế tiếp bị gì? Code em chặn trường hợp này ở đâu?

2. Lock lồng R1 (ceiling thấp) rồi R2 (ceiling cao), khi unlock R2, em ưu tiên task phải về mức nào, code em lưu ưu tiên cũ ở đâu? Thêm nữa, trên đơn nhân, nếu ceiling tính đúng thì wrapper còn cần semaphore bên dưới để loại trừ lẫn nhau không?

question week 3

1. Với baud và DMA buffer của em: bao lâu giữa hai lần ngắt half-transfer? Đó là gì của thời gian phản hồi tối đa cho phép của ISR + stream buffer, và nếu tăng baud gấp đôi con số đổi ra sao?

2. Vì sao bài này không dùng tickless có sẵn của port (SysTick) mà phải tự viết portSUPPRESS_TICKS_AND_SLEEP dùng RTC?

3. Chọn MSP hay PSP bằng cách nào? Stacked frame gồm các thanh ghi theo thứ tự nào, PC ở offset nào?

4. Một task bị block vĩnh viễn nằm trong danh sách nào của kernel (không phải xDelayedTaskList)? Làm sao biết nó chờ mutex nào?

Tài

1. Khung có W3E1_PAYLOAD_LEN_BYTES của em thì tổng bao nhiêu byte trên dây? Khi CRC sai, parser nên bỏ cả khung hay chỉ 1 byte rồi tìm lại 0xAA? Vì sao?

2. Nếu một byte đang truyền trên UART đúng lúc thoát STOP mà chưa khôi phục PLL thì sai gì? Và RTC alarm đánh thức STOP cần cấu hình EXTI line nào, cạnh nào?

3. Khi nào PC trong stacked frame trỏ sai dòng gây lỗi? Làm sao nhận biết và khắc phục để vẫn tìm được dòng lỗi?

4. Em dùng những đọc/lệnh GDB nào để chứng minh hệ thống vẫn chạy (không phải treo)? pxCurrentTCB lúc halt trỏ vào task nào?
