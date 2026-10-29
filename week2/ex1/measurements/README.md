# W2.1 – Dữ liệu đo

UART COM13, 115200 8N1. Logic analyzer: PulseView (Saleae Logic, 8 kênh), kênh SCL, SDA, EV (PB13), ER (PB14), FAULT (PB12).

| File | Nội dung |
|---|---|
| `uart_mpu6050_ok.png` | UART: `INIT,OK` và `ACCEL/GYRO,OK` (916 µs). |
| `w2e1_baseline.log.txt` | Log bản thường (125 dòng đầu là firmware cũ 11 cột; từ banner trở đi toàn `OK`). |
| `measure_v1.sr` | 12 MHz, chuỗi ngắt bình thường: ghi 1 byte 4 xung EV, đọc 6 byte 7 xung EV. |
| `measure_v1_boot.sr` | 12 MHz, chuỗi khởi động: `WHO_AM_I` 3 xung EV, `PWR_MGMT_1` 5 xung EV. |
| `measure_v1_inject.sr` + `w2e1_inject_la.log.txt` | 4 MHz, bản chèn lỗi; log 38 `RECOVERED`, streak 38. |
| `w2e1_inject_la2.log.txt` | Bản chèn lỗi lần 2; 38 `RECOVERED`, streak 38. |
| `measure_v4_inject.sr` + `w2e1_inject_la4.log.txt` | 4 MHz, bản chèn lỗi sạch nhất; 40 `RECOVERED`, streak 40; 4 sự cố trên dạng sóng (3 chậm, 1 nhanh). |

Mở `.sr` bằng PulseView (file `.pvs` đi kèm giữ cấu hình decoder). Kênh FAULT bám theo SDA vì PB12 nối vào SDA qua 220 Ω; xung chèn lỗi nhận biết bằng cột `INJ` trong log.
