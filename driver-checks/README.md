# Kiểm tra logic driver W2.1 trên PC

Thư mục này chứa mock thanh ghi STM32 và API FreeRTOS để kiểm tra trực tiếp
`week2/ex1/i2c1_driver.c`. Các header ở đây chỉ dùng cho test trên PC, không
được đưa vào include path khi build firmware STM32.

Chạy từ thư mục gốc repo bằng GCC cho máy tính, ví dụ trong PowerShell:

Test hiện dùng bộ tham số MSSV 2452882 (100 kHz, timeout 28 ms).
Nếu checkout mới chưa có `params.h`, sinh lại bằng lệnh đầu tiên bên dưới.

```powershell
python tools/gen_params.py 2452882 --only w2e1
New-Item -ItemType Directory -Force build | Out-Null
gcc -std=c11 -O2 -Wall -Wextra -Werror -Idriver-checks -Iweek2/ex1/include driver-checks/driver_test.c -o build/w2e1-driver-test.exe
if ($LASTEXITCODE -eq 0) { ./build/w2e1-driver-test.exe }
```

Test bao gồm transmit, receive 1/2/nhiều byte, NACK, BERR, ARLO, timeout,
hoàn tất ở biên timeout, ISR đến muộn, lấy/trả mutex, phục hồi SDA/SCL
và phép trừ cycle qua lần tràn DWT. `constants.h` chứa các mask bit lấy từ
header STM32F103xB trong repo.

Mock không tái tạo đầy đủ side effect thanh ghi, waveform, scheduler hay
priority inheritance. Số assertions và vòng recovery trong mô hình không
thay thế ARM build, đo logic analyzer hoặc thử phục hồi trên board thật.
