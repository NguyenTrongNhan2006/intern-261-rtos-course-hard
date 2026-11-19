# Hướng dẫn cho AI agent (Codex, Claude Code, …)

Repo bài tập khóa FreeRTOS "bản khó" trên STM32F103C8T6 (Blue Pill, 72 MHz, 20 KiB SRAM). MSSV **2452882**.
Đọc file này trước khi sửa bất cứ thứ gì; quy tắc chấm nằm ở `README.md` và `SYLLABUS.md`.

## Quy tắc cứng (vi phạm = mất điểm phần code hoặc vấn đáp)

- Không HAL/LL, không CMSIS-OS, không file do công cụ sinh (CubeMX, Makefile). Từ `week1/ex4` trở đi chỉ cấp phát tĩnh (`*Static`).
  Kiểm bằng `bash tools/check_api.sh <thư mục>`; script lọc comment trước khi dò nên được phép nhắc tên API cấm trong comment, nhưng **không** trong string literal.
- C99, `-Wall -Wextra -Werror -pedantic`. Chỉ dùng thanh ghi qua `stm32f1xx.h` và mã nguồn FreeRTOS-Kernel.
- Mỗi lần AI hỗ trợ một bài phải có mục tương ứng trong `AI_LOG.md` (prompt tóm tắt, AI làm gì, sinh viên làm gì). Thiếu `AI_LOG.md` → vấn đáp = 0.
- `PREDICTION.md` phải viết **trước** khi đo. Sau khi có số đo thì không sửa số dự đoán cho khớp; mọi chỉnh sửa trước đó ghi vào bảng "Nhật ký sửa đổi" ở đầu file.
- Không bịa số đo. Mọi con số trong báo cáo gắn nhãn **Đo** (trích file/bài nguồn), **Suy luận** (tính từ số đo) hoặc **Ước lượng** (chưa đo). Build/host test không thay cho phép đo trên board.
- Không commit file tạm, ảnh chụp kiểm tra, script dùng một lần vào thư mục bài.

## Môi trường

- `include/params.h` và `PARAMS.md` của mỗi bài **bị `.gitignore`**: sinh lại bằng `python3 tools/gen_params.py 2452882 [--only w4e1,w4e2]` (tất định theo MSSV). Link tới các file này không mở được trên GitHub.
- Toolchain: `arm-none-eabi-gcc` (cloud container: `apt-get install -y gcc-arm-none-eabi`).
- Ngôn ngữ: tài liệu (`*.md`) tiếng Việt; comment trong code và `params.h` tiếng Anh. Số thập phân trong `.md` dùng dấu phẩy, phân cách hàng nghìn bằng dấu chấm (`1.376`, `31,29 %`).
- Khi sinh Markdown/LaTeX bằng Python, dùng raw string (`r'''…'''`): `\a`, `\t`, `\b` trong chuỗi thường sẽ biến `\approx`, `\text` thành ký tự điều khiển.

## week4/ex1 — W4.1 thiết kế mixed-criticality (chỉ trên giấy)

Không tạo `.c`, không viết thân hàm. Code duy nhất được phép là kiểu và bảng `static const` trong `include/mixed_criticality_design.h`.

**Nguồn số duy nhất (SSOT)** là `include/mixed_criticality_design.h`: `W4E1_RTA_TABLE` (T, D, C_HI, R_HI, priority), `W4E1_C_LO_US`, `W4E1_RAM_BUDGET_TABLE`, `W4E1_FAILURE_MODE_TABLE` và các `W4E1_DESIGN_*`. Các file sau phải trích đúng các số đó:

| File | Chứa |
|---|---|
| `PREDICTION.md` | Bảng RTA, nguồn gốc từng C_i (có nhãn), các bước lặp RTA, ngân sách, rủi ro, nhật ký sửa đổi |
| `REPORT.md` | Design doc: 3 sơ đồ Mermaid, bảng RTA/CPU, AMC-rtb, RAM, failure mode, §5 audit (F1–F26) |
| `w4e1_architecture.html` | Sơ đồ SVG tự chứa (pan/zoom, FSM, bảng tra cứu) |
| `w4e1_architecture.graphml` | Cùng sơ đồ cho yEd, cùng toạ độ với HTML |

**Kiểm tra bắt buộc sau mọi thay đổi** (tính lại RTA LO/HI, thời gian hoàn tất `imu`, CPU, RAM, đối chiếu số ở cả 5 file, gcc, `check_api.sh`):

```bash
python3 tools/check_w4e1.py
```

**Đổi một con số** (C, T, kích thước buffer…): sửa header trước → chạy `tools/check_w4e1.py` để biết R/CPU/RAM mới → cập nhật bảng và các bước lặp trong PREDICTION, §2–§5 REPORT, nhãn node/bảng trong HTML, nhãn node trong GraphML, khối Mermaid → chạy lại script tới khi PASS → thêm dòng vào nhật ký sửa đổi và một mục F trong REPORT §5.

Các quyết định thiết kế đã chốt (đừng đảo ngược mà không hỏi người dùng):

- `C_HI(control) = 97 µs` = C_LO 70 + nhánh stale/safe-hold 11 + 1 ngắt USART1 RX 16.
- `imu` đọc burst 14 byte (accel + gyro) ở **I2C1 400 kHz**; chờ bus ≈ 433 µs; hoàn tất 862 µs; được `TIM3` đánh thức mỗi 2 tick, cùng pha với `control`.
- Vòng DMA RX **128 B** (HT/TC mỗi 64 B) để ISR UART ≤ 16 µs.
- Báo động jitter ở 100 µs, trần cứng 113 µs. Ngưỡng mẫu IMU cũ là > 3 tick. Mặt nạ heartbeat ở HI-mode chỉ gồm `control` + `imu`.

Quy ước sơ đồ:

- 4 làn theo thứ tự: Hardware ISRs → HI tasks → Static IPC → LO tasks. Đường nối vuông góc, không cạnh nào cắt nhau, nhãn nằm trên đúng cạnh của nó.
- Heartbeat vẽ bằng nhãn `HB` trên task, không vẽ thành cạnh: cặp {control, imu} × {SPSC, EventGroup} giữa hai làn liền kề luôn sinh giao cắt.
- HTML và GraphML dùng chung toạ độ. Khi thêm cạnh, chọn khe trống và kiểm bằng render thật (Chromium/Playwright có sẵn trong cloud container).
- Mermaid: không có mũi tên ngược đáng tin cậy (`A <-- B` lỗi cú pháp, `<---` mất đầu mũi tên), nên luồng đi lên viết bằng `<-->` khi thật sự hai chiều, hoặc đường không mũi tên cho đọc không khóa. Dùng `curve: step`/`stepBefore`, tiêu đề làn là node riêng (`:::lane`), và `~~~` giữa các node tiêu đề để ghim thứ tự làn. Tránh `stateDiagram` vì nền nhãn bán trong suốt bị đường kẻ gạch qua.

Khoảng trống dữ liệu còn mở (phải đo ở W4.2/W4.3, đừng trình bày như đã đo): phần lớn các thành phần C_i (xem nhãn Ước lượng trong PREDICTION), nhánh phát 9 xung SCL, số lần ISR và thời gian bus khi đọc 14 byte ở 400 kHz, sườn lên SCL ở 400 kHz, critical section của kernel trong cửa sổ `control`, `dma_late`/`dropped` ở 1 Mbps với vòng 128 B.
