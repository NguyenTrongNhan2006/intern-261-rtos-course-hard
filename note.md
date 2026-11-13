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
