# W1E3 V2: hiệu chỉnh workload và đo IRQ

V1 đã có số đo thực. V2 thay đổi code và timing, phải đo mới trước khi
áp số liệu. Dữ liệu/ELF/map/source V1 được backup và checkpoint riêng.

## 1. Cách chạy

Từ repo root sau khi tích hợp:

```powershell
$w1e3Python = 'C:\Users\Nhan\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$w1e3Ninja = 'C:/Users/Nhan/AppData/Local/Microsoft/WinGet/Packages/Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe/ninja.exe'
$w1e3OpenOcd = 'C:/Users/Nhan/AppData/Local/Microsoft/WinGet/Packages/xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe/xpack-openocd-0.12.0-7/bin/openocd.exe'
cmake -S . -B build/w1e3-v2-O2 -G Ninja "-DCMAKE_MAKE_PROGRAM=$w1e3Ninja" "-DOPENOCD_EXECUTABLE=$w1e3OpenOcd" "-DCMAKE_TOOLCHAIN_FILE=N:/Embedded Portfolio/intern-261-rtos-course-hard/cmake/arm-none-eabi.cmake" "-DCMAKE_BUILD_TYPE=Debug" "-DCMAKE_C_FLAGS_DEBUG=-O2 -g3"
cmake --build build/w1e3-v2-O2 --target w1e3
cmake --build build/w1e3-v2-O2 --target w1e3_flash
& $w1e3Python tools/capture_w1e3.py --port COM19 --out week1/ex3/measurements/trace_v2_O2.csv --timeout 90
```

Khi port mở, nhấn RESET. Hiệu chỉnh diễn ra trước scheduler, có thể cần
vài giây trước cửa sổ 6 giây chạy task. Chờ DONE; timeout không là log đủ.
Phân tích bằng script mới, không dùng parser JOB của V1:

```powershell
& $w1e3Python tools/analyze_w1e3_v2.py week1/ex3/measurements/trace_v2_O2.csv --json-out week1/ex3/measurements/results_v2_O2.json
```

O0 dùng flags -O0 -g3, thư mục build/w1e3-v2-O0 và tên trace_v2_O0.csv.
GPIO mapping giữ PA4..PA8 = Idle/tau0/tau1/tau2/tau3, UART PA9 115200.
LA mới lưu tại week1/ex3/measurements/capture_v2_O0.sr và
week1/ex3/measurements/capture_v2_O2.sr; không ghi đè V1.

## 2. Framing

CAL: tag,target_cycles,iterations,min_cycles,max_cycles (8 samples).

JOB2: tag,sequence,release,finish,work_wall,tick_cycles,pend_cycles,
tick_count,pend_count,resumes. Các trường thời gian đều là cycles.

IRQ: kind,count,min,max,total; kind 0 là SysTick, 1 là PendSV.

IRQPROBE: min,max của cặp enter/exit rỗng trước scheduler. HEAP: free,minfree.

## 3. Giới hạn và kết luận được phép

Vòng workload chạy đủ instruction count dù bị preempt. Calibration kiểm
tra uncontended timing bằng DWT; thay đổi fetch phase sau preemption vẫn
có thể tạo chi phí nhỏ, không tự suy ra WCET hardware tuyệt đối.

IRQ bracket đo phần mềm giữa hai timestamp trong helper; hardware
entry/exit và mép wrapper không nằm trọn trong đó. PendSV bao gồm trace
hooks, scheduler và software save/restore; OUT->IN là một phần của nó.
Profiler tự thêm overhead, empty probe chỉ là tham chiếu observer cost.
Residual phải được giữ đúng là residual, không tự gán toàn bộ cho một ISR.

Analyzer kiểm tra đủ 205 job, shared release timestamp, sequence, bounds
calibration và tính nhất quán counter. Nếu worst window có carry-in thì
không áp cách trừ C của các job đầy đủ.

Toàn bộ dữ liệu/Gantt/JSON/log ở [measurements/](measurements/README.md).
Các tên chuẩn đã chứa số đo được kiểm chứng; lượt đo lại dùng tên mới để giữ dữ liệu cũ.
