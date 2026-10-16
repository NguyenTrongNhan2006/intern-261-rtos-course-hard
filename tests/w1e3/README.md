# W1E3 V2 verification

From repository root on this Windows host (Python stdlib, native GCC):

```powershell
$w1e3Python = 'C:\Users\Nhan\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
New-Item -ItemType Directory -Force build/w1e3-v2-host | Out-Null
gcc -std=c11 -Wall -Wextra -Werror -I tests/w1e3/host -I week1/ex3/include -I week1/ex2/include -I platform/include tests/w1e3/host/test_measure.c -o build/w1e3-v2-host/test_measure.exe
& .\build\w1e3-v2-host\test_measure.exe
& $w1e3Python tests/w1e3/test_analysis.py
& $w1e3Python tests/w1e3/check_elf.py
```

The native harness compiles the actual rta_measure.c with fake DWT/UART/
FreeRTOS primitives. It tests calibration/search, fixed iteration count
under a simulated interrupt, partial SysTick clipping at release, counters
and complete 205-job framing. It does NOT emulate exception entry/return
or validate hardware timing. ELF checks verify vector addresses and wrapper
call structure for the separate O0/O2 builds. Hardware logs establish actual
execution of the wrappers; synthetic CSV tests are not hardware evidence.

The port uses indirect handler routing. configCHECK_HANDLER_INSTALLATION=0
is required by the ARM_CM3 port for this routing; main separately asserts
SVC/PendSV/SysTick vector addresses before the scheduler starts.

Analyze the measured logs with:

```powershell
& $w1e3Python tools/analyze_w1e3_v2.py week1/ex3/trace_v2_O0.csv --json-out week1/ex3/results_v2_O0.json
& $w1e3Python tools/analyze_w1e3_v2.py week1/ex3/trace_v2_O2.csv --json-out week1/ex3/results_v2_O2.json
```
