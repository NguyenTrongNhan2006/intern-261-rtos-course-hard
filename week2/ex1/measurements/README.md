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
| `Session 1` | Session PulseView 4 MHz; dữ liệu sample giống `measure_v4_inject.sr` (đã đối chiếu SHA-256 phần sample), khác container ZIP. Giữ nguyên bản người thực hiện cung cấp; không tính là lượt đo độc lập. |
| `normal_ev.png` | Dựng từ sample `measure_v1.sr`: ghi register 4 EV rồi đọc 6 byte 7 EV. |
| `boot_ev.png` | Dựng từ sample `measure_v1_boot.sr`: chọn WHO_AM_I, đọc 0x68 và ghi PWR_MGMT_1. |
| `recovery_fast.png` | Dựng từ sample `measure_v4_inject.sr`: gián đoạn transaction, ER và START/STOP GPIO. |
| `recovery_slow.png` | Hai cửa sổ cùng capture v4: xung START/STOP giả và recovery sau 92.222 ms. |

Mở `.sr` bằng PulseView (file `.pvs` đi kèm giữ cấu hình decoder). Kênh FAULT bám theo SDA vì PB12 nối vào SDA qua 220 Ω; xung chèn lỗi nhận biết bằng cột `INJ` trong log.

Rà dữ liệu ngày 30/10/2026: mọi RECOVERED trong ba log có OK kế tiếp và tăng streak. Mọi `PULSES` vẫn bằng 0: chưa kiểm chứng nhánh phát 1–9 xung SCL để gỡ slave giữ SDA LOW. File boot có một xung ER ở 417.853 ms ngoài đoạn khởi tạo đã chọn; chỉ `measure_v1.sr` không có xung ER trong toàn capture. Xem các số đo theo sample và giới hạn trong [REPORT.md](../REPORT.md).
