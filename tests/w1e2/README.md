# W1E2 host regression checks

These checks execute the real `tracer.c` with fake Cortex-M registers,
FreeRTOS tag getters and UART. They validate logic and output values,
not interrupt timing, MMIO semantics, or physical measurement.

From the repository root with a native GCC and Python 3:

```text
python -B -m unittest discover -s tests/w1e2 -p "test_*.py" -v
cmake --preset w1e2-o2
cmake --build --preset w1e2-o2
gcc -std=c11 -Wall -Wextra -Werror -O2 -Itests/w1e2/stubs -Iweek1/ex2/include -Iplatform/include tests/w1e2/tracer_host_test.c -o build/w1e2-current-O2/tracer_host_test.exe
```

Run `build/w1e2-current-O2/tracer_host_test.exe` on Windows; on other
hosts choose the matching executable name. Repeat with `-O0`, and with
`-DTRACER_FIRST_PIN=6u -DTRACER_LAST_PIN=8u` to test a changed GPIO range.

The C harness tests unknown/invalid tag callbacks without a hook assert,
thread/ISR getter selection, private ring producer freeze and dump,
64-bit dump values after two observed 32-bit wraps, overflow, and masks.
The Python tests cover chunk ordering, strict UART pairing, duplicate
captures, 64-bit input, raw-wrap rejection, data mismatch and failed
correlation bounds. No test edits a measured artifact.
