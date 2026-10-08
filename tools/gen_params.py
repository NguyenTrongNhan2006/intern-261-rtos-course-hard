#!/usr/bin/env python3
"""Deterministic per-student parameter generator (BẢN KHÓ).

Usage:
    python tools/gen_params.py <MSSV> [--only w1e3,w3e1] [--root DIR] [--list]

For every registered exercise key ("w<N>e<M>") this writes:
    weekN/exM/include/params.h   (English comments; student firmware input)
    weekN/exM/PARAMS.md          (Vietnamese; student-facing explanation)

Determinism: every value is derived from sha256(f"{mssv}:{key}:<tag>") so the
same MSSV always reproduces the same files, independently of run order or
which other exercises are also being generated.

Extending: add a new exercise by writing a function and decorating it with
@exercise("w<N>e<M>") - see w1e3() below for the required shape:
    def wNeM(r: random.Random, mssv: str) -> (defines: list[str], doc: list[str])
`r` is a random.Random seeded only for this (mssv, key) pair; never reuse the
module-level `random` functions directly or determinism breaks.

Must run cleanly even when no weekN/ folders exist yet: output directories
are created with os.makedirs(..., exist_ok=True).
"""
import argparse
import hashlib
import os
import re
import sys
from random import Random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

REGISTRY = {}  # key -> callable(r, mssv) -> (defines: list[str], doc: list[str])


def exercise(key):
    if not re.fullmatch(r"w\d+e\d+", key):
        raise ValueError(f"bad exercise key: {key!r}")

    def deco(fn):
        REGISTRY[key] = fn
        return fn

    return deco


def key_to_path(key):
    m = re.fullmatch(r"w(\d+)e(\d+)", key)
    return f"week{m.group(1)}", f"ex{m.group(2)}"


def seeded_rng(mssv, key, tag=""):
    """Independent, reproducible RNG stream per (mssv, key, tag)."""
    digest = hashlib.sha256(f"{mssv}:{key}:{tag}".encode("utf-8")).digest()
    seed = int.from_bytes(digest[:8], "big")
    return Random(seed)


def bucket_roll(mssv, key, tag, pct, out_of=100):
    """Deterministic True/False gate, independent of any Random() calls the
    exercise function itself makes (so adding/removing random draws inside
    the exercise never shifts which IDs land in the bucket)."""
    digest = hashlib.sha256(f"{mssv}:{key}:{tag}".encode("utf-8")).digest()
    return (int.from_bytes(digest[:8], "big") % out_of) < pct


# --------------------------------------------------------------------------
# W1.1 - per-ID task-tag GPIO pin + blink periods + 72 MHz verification bound
# --------------------------------------------------------------------------
# Pins free on a Blue Pill after USART1 (PA9/PA10), SWD (PA13/PA14) and the
# onboard LED (PC13) are reserved; safe to drive as push-pull outputs.
W1E1_CANDIDATE_PINS = [
    ("GPIOA", 4), ("GPIOA", 5), ("GPIOA", 6), ("GPIOA", 7), ("GPIOA", 8), ("GPIOA", 15),
    ("GPIOB", 0), ("GPIOB", 1), ("GPIOB", 3), ("GPIOB", 4), ("GPIOB", 5),
    ("GPIOB", 10), ("GPIOB", 11),
]


@exercise("w1e1")
def w1e1(r, mssv):
    port, pin = r.choice(W1E1_CANDIDATE_PINS)
    period_a_ms = r.randint(200, 500)
    period_b_ms = r.randint(600, 900)
    freq_tolerance_ppm = r.randint(50, 300)
    tolerance_hz = round(1000 * freq_tolerance_ppm * 1e-6, 3)

    defines = [
        "/* Task-tag GPIO pin used by the two blinky tasks (push-pull output,",
        "   configure clock + CRL/CRH yourself). Blink periods in ms. */",
        f'#define W1E1_TAG_GPIO_PORT {port}',
        f"#define W1E1_TAG_GPIO_PIN {pin}",
        f"#define W1E1_TASK_A_PERIOD_MS {period_a_ms}",
        f"#define W1E1_TASK_B_PERIOD_MS {period_b_ms}",
        "/* TIM-generated 1 kHz verification pulse: allowed deviation from",
        "   exactly 1000 Hz when 72 MHz is correctly configured. */",
        f"#define W1E1_CLOCK_TOLERANCE_PPM {freq_tolerance_ppm}",
    ]
    doc = [
        "# W1.1 - Tham số bring-up",
        "",
        f"- Chân GPIO dùng để tag task (blink + đo logic analyzer): **{port} pin {pin}**.",
        f"- Chu kỳ blink task A: **{period_a_ms} ms**, task B: **{period_b_ms} ms**.",
        f"- Xung xác minh 72 MHz xuất ra từ TIM ở 1 kHz; sai số cho phép: "
        f"**{freq_tolerance_ppm} ppm** (~±{tolerance_hz} Hz quanh 1000 Hz).",
        "- Đo tần số thực tế bằng logic analyzer và ghi vào SETUP.md/REPORT.md;",
        "  vượt ngưỡng trên nghĩa là cấu hình PLL/flash-latency sai.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W1.3 - RTA task set (4 tasks, implicit deadlines D = T), ~30% unschedulable
# --------------------------------------------------------------------------
LL_BOUND_N4 = 4 * (2 ** 0.25 - 1)  # Liu-Layland sufficient bound for n=4 ~= 0.7568
CANDIDATE_PERIODS_MS = [40, 50, 60, 75, 80, 100, 120, 150, 200, 250, 300, 400]


def _rta_feasible(tasks_hi_to_lo):
    """Classic response-time analysis for fixed-priority, implicit deadlines.
    tasks_hi_to_lo: list of {"C": float_ms, "T": float_ms}, index 0 = highest
    priority (shortest period, Rate Monotonic). Returns True iff every task
    meets R_i <= T_i."""
    import math

    n = len(tasks_hi_to_lo)
    for i in range(n):
        C = tasks_hi_to_lo[i]["C"]
        Ri = C
        for _ in range(1000):
            interference = sum(
                math.ceil(Ri / tasks_hi_to_lo[j]["T"]) * tasks_hi_to_lo[j]["C"]
                for j in range(i)
            )
            Rn = C + interference
            if Rn > tasks_hi_to_lo[i]["T"]:
                return False
            if Rn == Ri:
                break
            Ri = Rn
        else:
            return False  # did not converge inside the hyperperiod -> treat as miss
    return True


@exercise("w1e3")
def w1e3(r, mssv):
    unschedulable = bucket_roll(mssv, "w1e3", "bucket", 30)

    periods = sorted(r.sample(CANDIDATE_PERIODS_MS, 4))
    if unschedulable:
        target_u = r.uniform(1.05, 1.35)  # U > 1 => infeasible on 1 CPU, period = proof
    else:
        target_u = r.uniform(0.40, 0.95 * LL_BOUND_N4)  # safely under the LL bound

    weights = [r.uniform(0.3, 1.0) for _ in range(4)]
    wsum = sum(weights)
    tasks = []
    for T, w in zip(periods, weights):
        u_i = (w / wsum) * target_u
        C_ms = max(0.05, round(u_i * T, 3))
        tasks.append({"T": T, "C": C_ms, "D": T})

    # Priority order for RM = ascending period; sanity-check our own math so a
    # generator bug never ships a task set that silently violates the rule.
    hi_to_lo = sorted(tasks, key=lambda t: t["T"])
    feasible = _rta_feasible(hi_to_lo)
    total_u = sum(t["C"] / t["T"] for t in tasks)
    if unschedulable:
        assert total_u > 1.0 and not feasible, "generator bug: expected infeasible set"
    else:
        assert total_u <= LL_BOUND_N4 and feasible, "generator bug: expected feasible set"

    defines = [
        "/* 4-task periodic set, implicit deadlines (D_i = T_i). */",
        "/* C is a calibrated DWT busy-wait, in microseconds. */",
        "#define W1E3_NUM_TASKS 4",
        "",
        "static const unsigned long W1E3_PERIOD_MS[W1E3_NUM_TASKS] = {"
        + ", ".join(str(t["T"]) for t in tasks) + "};",
        "static const unsigned long W1E3_DEADLINE_MS[W1E3_NUM_TASKS] = {"
        + ", ".join(str(t["D"]) for t in tasks) + "};",
        "static const unsigned long W1E3_BUSY_WAIT_US[W1E3_NUM_TASKS] = {"
        + ", ".join(str(round(t["C"] * 1000)) for t in tasks) + "};",
    ]

    doc = [
        "# W1.3 - Tham số phân tích lập lịch (RTA)",
        "",
        "| Task | T (ms) | D (ms) | C busy-wait (us) |",
        "|---|---|---|---|",
    ]
    for i, t in enumerate(tasks):
        doc.append(f"| τ{i} | {t['T']} | {t['D']} | {round(t['C'] * 1000)} |")
    doc += [
        "",
        "Ghi chú:",
        "- Đây là bộ 4 task với deadline ngầm định D_i = T_i.",
        "- Đề KHÔNG tiết lộ tập này có schedulable hay không theo RM. Sinh viên phải",
        "  tự tính utilization, so với giới hạn Liu-Layland, rồi chạy RTA bằng tay để",
        "  kết luận chắc chắn (LL chỉ là điều kiện đủ, không phải cần).",
        "- Nếu kết luận KHÔNG schedulable: phải đề xuất cách sửa (đổi T, giảm C,",
        "  đổi priority...) và chứng minh lại bằng RTA.",
        "- C sinh ra bằng vòng lặp busy-wait hiệu chỉnh qua DWT->CYCCNT ở 72 MHz;",
        "  tự quy đổi us -> số chu kỳ cần đếm.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W1.2 - per-ID jitter tolerance for tracer-vs-logic-analyzer correlation
# --------------------------------------------------------------------------
@exercise("w1e2")
def w1e2(r, mssv):
    jitter_tolerance_us = r.randint(5, 20)
    jitter_max_cycles_o2 = r.randint(80, 400)

    defines = [
        "/* Max allowed mismatch when correlating tracer timestamps against the",
        "   logic-analyzer capture, and the pass/fail cap on measured context-switch",
        "   jitter (max - min) when built at -O2. */",
        f"#define W1E2_CORRELATION_TOLERANCE_US {jitter_tolerance_us}",
        f"#define W1E2_CTXSW_JITTER_MAX_CYCLES_O2 {jitter_max_cycles_o2}",
    ]
    doc = [
        "# W1.2 - Tham số tracer / Gantt",
        "",
        f"- Sai số cho phép khi đối chiếu Gantt từ tracer với ảnh logic analyzer: "
        f"**{jitter_tolerance_us} us**.",
        f"- Jitter context-switch tối đa chấp nhận được ở -O2 (max - min): "
        f"**{jitter_max_cycles_o2} chu kỳ**.",
        "- Vượt quá hai ngưỡng trên coi là KHÔNG đạt tiêu chí Output của W1.2;",
        "  giải thích định lượng nguyên nhân (tick, context switch, ISR) trong REPORT.md.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W3.1 - per-ID UART baud + CRC stress parameters (feeds tools/uart_stress.py)
# --------------------------------------------------------------------------
BAUD_CHOICES = [460800, 500000, 576000, 921600, 1000000]


@exercise("w3e1")
def w3e1(r, mssv):
    baud = r.choice(BAUD_CHOICES)
    payload_len = r.randint(16, 64)
    frame_rate_hz = r.randint(20, 200)

    defines = [
        "/* UART1 RX DMA + IDLE line stress parameters. */",
        f"#define W3E1_BAUD_BPS {baud}UL",
        f"#define W3E1_PAYLOAD_LEN_BYTES {payload_len}",
        f"#define W3E1_FRAME_RATE_HZ {frame_rate_hz}",
        "#define W3E1_STRESS_DURATION_MIN 10",
    ]
    doc = [
        "# W3.1 - Tham số UART DMA + IDLE line",
        "",
        f"- Baud rate được giao: **{baud} bps**.",
        f"- Kích thước payload mỗi frame: **{payload_len} bytes**.",
        f"- Tốc độ gửi frame: **{frame_rate_hz} Hz**.",
        "- Thời lượng stress test: **10 phút**, 0 lỗi CRC, 0 mất byte (dùng "
        "`tools/uart_stress.py --baud "
        f"{baud} --payload-size {payload_len} --rate-hz {frame_rate_hz} "
        "--duration-sec 600`).",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W3.2 - per-ID tickless-idle drift bound
# --------------------------------------------------------------------------
@exercise("w3e2")
def w3e2(r, mssv):
    drift_ppm_bound = r.randint(50, 500)
    window_min = 10
    drift_ms_bound = round(drift_ppm_bound * 1e-6 * window_min * 60 * 1000, 1)

    defines = [
        "/* Max allowed clock drift vs PC wall clock over the test window. */",
        f"#define W3E2_DRIFT_BOUND_PPM {drift_ppm_bound}",
        f"#define W3E2_DRIFT_TEST_WINDOW_MIN {window_min}",
    ]
    doc = [
        "# W3.2 - Tham số trôi tick (tickless idle)",
        "",
        f"- Ngưỡng trôi cho phép: **{drift_ppm_bound} ppm** "
        f"(~{drift_ms_bound} ms trong {window_min} phút).",
        "- Đo trôi giờ thực tế so với giờ PC trong cửa sổ trên; nếu vượt ngưỡng,",
        "  giải thích nguồn trôi (RTC oscillator tolerance, bù tick làm tròn...).",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W1.4 - per-ID heap_4 fragmentation sequence + static RAM budget
# --------------------------------------------------------------------------
W1E4_BLOCK_SIZES_BYTES = [24, 40, 56, 72, 96, 128, 160, 200, 256, 320]


@exercise("w1e4")
def w1e4(r, mssv):
    n_blocks = r.randint(6, 9)
    sizes = [r.choice(W1E4_BLOCK_SIZES_BYTES) for _ in range(n_blocks)]
    # Free order: a permutation that is NOT sorted (forces holes instead of a
    # clean LIFO collapse), fixed per student so the fragmentation log is
    # reproducible across re-runs.
    free_order = list(range(n_blocks))
    while True:
        r.shuffle(free_order)
        if free_order != list(range(n_blocks)) and free_order != list(reversed(range(n_blocks))):
            break
    repeat_cycles = r.randint(3, 6)
    ram_budget_tolerance_bytes = r.randint(32, 256)

    defines = [
        "/* heap_4 fragmentation experiment: allocate W1E4_NUM_BLOCKS blocks of",
        "   the given sizes (pvPortMalloc), then free them in W1E4_FREE_ORDER",
        "   (index into the allocation array, not size), repeat the whole",
        "   cycle W1E4_REPEAT_CYCLES times, and log xPortGetFreeHeapSize() /",
        "   xPortGetMinimumEverFreeHeapSize() after every step. */",
        f"#define W1E4_NUM_BLOCKS {n_blocks}",
        "static const unsigned int W1E4_BLOCK_SIZE_BYTES[W1E4_NUM_BLOCKS] = {"
        + ", ".join(str(s) for s in sizes) + "};",
        "static const unsigned int W1E4_FREE_ORDER[W1E4_NUM_BLOCKS] = {"
        + ", ".join(str(i) for i in free_order) + "};",
        f"#define W1E4_REPEAT_CYCLES {repeat_cycles}",
        "/* After switching to configSUPPORT_DYNAMIC_ALLOCATION=0, allowed",
        "   mismatch between your hand-tallied RAM budget and the linker .map. */",
        f"#define W1E4_RAM_BUDGET_TOLERANCE_BYTES {ram_budget_tolerance_bytes}",
    ]
    doc = [
        "# W1.4 - Tham số thí nghiệm phân mảnh heap + ngân sách RAM",
        "",
        "| # | Size (bytes) |",
        "|---|---|",
    ]
    for i, s in enumerate(sizes):
        doc.append(f"| {i} | {s} |")
    doc += [
        "",
        f"- Thứ tự free (theo chỉ số cấp phát ở trên): **{free_order}**.",
        f"- Lặp lại toàn bộ chu trình cấp phát/giải phóng **{repeat_cycles} lần**,",
        "  ghi `xPortGetFreeHeapSize()` và `xPortGetMinimumEverFreeHeapSize()`",
        "  sau mỗi bước vào log.",
        f"- Sau khi chuyển sang `configSUPPORT_DYNAMIC_ALLOCATION=0`: sai số cho",
        f"  phép giữa bảng RAM tự tính và file `.map` là **{ram_budget_tolerance_bytes} bytes**.",
        "- Giải thích vì sao thứ tự free ở trên gây phân mảnh (hay không) dựa",
        "  trên thuật toán coalescing của heap_4 (best-fit theo địa chỉ).",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W2.1 - per-ID I2C1 speed/timeout + bus-recovery repeat count
# --------------------------------------------------------------------------
W2E1_SPEED_CHOICES_HZ = [100000, 400000]


@exercise("w2e1")
def w2e1(r, mssv):
    speed_hz = r.choice(W2E1_SPEED_CHOICES_HZ)
    timeout_ms = r.randint(10, 50)
    # Syllabus pass criterion is ">= 10 lan lien tiep"; give every ID some
    # margin above the floor so the bar isn't exactly the minimum for all.
    recovery_repeat_count = r.randint(10, 20)

    defines = [
        "/* I2C1 master speed (Standard or Fast Mode), soft timeout for a",
        "   transaction, and the minimum number of consecutive SDA-glitch",
        "   recoveries the driver must survive without a reset. */",
        f"#define W2E1_I2C_SPEED_HZ {speed_hz}UL",
        f"#define W2E1_I2C_TIMEOUT_MS {timeout_ms}",
        f"#define W2E1_RECOVERY_REPEAT_COUNT {recovery_repeat_count}",
    ]
    doc = [
        "# W2.1 - Tham số driver I2C1",
        "",
        f"- Tốc độ I2C1 được giao: **{speed_hz} Hz** "
        f"({'Standard Mode' if speed_hz == 100000 else 'Fast Mode'}).",
        f"- Timeout mềm cho 1 transaction: **{timeout_ms} ms**.",
        f"- Số lần phục hồi bus liên tiếp phải đạt được (rút dây SDA giữa transaction): "
        f"**{recovery_repeat_count}** (đề yêu cầu tối thiểu 10).",
        "- Không đạt đủ số lần trên liên tiếp (dù chỉ 1 lần phải reset MCU) là KHÔNG đạt",
        "  tiêu chí Output của W2.1.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W2.2 - per-ID SPSC ring buffer capacity + stress/benchmark iteration counts
# --------------------------------------------------------------------------
W2E2_CAPACITY_CHOICES = [64, 128, 256, 512]


@exercise("w2e2")
def w2e2(r, mssv):
    capacity = r.choice(W2E2_CAPACITY_CHOICES)
    # Syllabus explicitly requires exactly 1 million elements for the proof
    # of correctness; keep this fixed (not per-ID) so "no loss/duplication"
    # means the same thing for every student.
    stress_count = 1_000_000
    bench_iterations = r.randint(2000, 8000)

    defines = [
        "/* SPSC ring buffer capacity (power of 2), element count for the",
        "   correctness stress test, and iteration count for the cycle",
        "   benchmark against xQueueSendFromISR/xQueueReceive. */",
        f"#define W2E2_RING_CAPACITY {capacity}u",
        f"#define W2E2_STRESS_COUNT {stress_count}u",
        f"#define W2E2_BENCH_ITERATIONS {bench_iterations}u",
    ]
    doc = [
        "# W2.2 - Tham số SPSC ring buffer",
        "",
        f"- Dung lượng ring buffer (lũy thừa của 2): **{capacity}** phần tử.",
        f"- Số phần tử stress test chứng minh đúng (không mất/lặp): **{stress_count:,}**.",
        f"- Số lần lặp benchmark cycle (ring buffer vs `xQueueSendFromISR`/`xQueueReceive`): "
        f"**{bench_iterations}**.",
        "- Bất kỳ lần mất/lặp nào trong stress test đều coi là KHÔNG đạt tiêu chí Output.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W2.3 - per-ID 4-task/2-mutex priority-inversion/chained-blocking/deadlock set
# --------------------------------------------------------------------------
W2E3_PRIORITY_POOL = [2, 3, 4, 5, 6, 7]


@exercise("w2e3")
def w2e3(r, mssv):
    # 4 distinct priorities, sorted ascending; index 3 = tau_H (highest).
    priorities = sorted(r.sample(W2E3_PRIORITY_POOL, 4))
    lock_hold_ms_a = r.randint(15, 40)
    lock_hold_ms_b = r.randint(15, 40)

    defines = [
        "/* 4-task priority set for the inversion / chained-blocking / deadlock",
        "   scenarios, ascending: [tau_L, tau_M1, tau_M2, tau_H]. Lock hold",
        "   times (ms, busy-wait) for the two protected resources. */",
        "#define W2E3_NUM_TASKS 4",
        "static const unsigned long W2E3_TASK_PRIORITY[W2E3_NUM_TASKS] = {"
        + ", ".join(str(p) for p in priorities) + "};",
        f"#define W2E3_LOCK_HOLD_MS_A {lock_hold_ms_a}",
        f"#define W2E3_LOCK_HOLD_MS_B {lock_hold_ms_b}",
    ]
    doc = [
        "# W2.3 - Tham số bộ task inversion / chained blocking / deadlock",
        "",
        "| Vai trò | tau_L | tau_M1 | tau_M2 | tau_H |",
        "|---|---|---|---|---|",
        "| Priority | " + " | ".join(str(p) for p in priorities) + " |",
        "",
        f"- Thời gian giữ khóa A (busy-wait): **{lock_hold_ms_a} ms**.",
        f"- Thời gian giữ khóa B (busy-wait): **{lock_hold_ms_b} ms**.",
        "- tau_H là task ưu tiên cao nhất (chỉ số cuối); tau_L thấp nhất (chỉ số đầu).",
        "- Dùng đúng 4 priority này cho cả 3 hiện tượng và cho bản dùng Priority Ceiling,",
        "  để bảng so sánh thời gian block công bằng giữa 3 cơ chế.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W2.4 - per-ID barrier size + sync bitmask/timeout
# --------------------------------------------------------------------------
@exercise("w2e4")
def w2e4(r, mssv):
    num_tasks = r.randint(3, 6)
    sync_bits_mask = (1 << num_tasks) - 1
    sync_timeout_ms = r.randint(100, 500)

    defines = [
        "/* Barrier participant count (xEventGroupSync), one bit per task,",
        "   and the per-task sync timeout. */",
        f"#define W2E4_NUM_TASKS {num_tasks}",
        f"#define W2E4_SYNC_BITS_MASK 0x{sync_bits_mask:02X}UL",
        f"#define W2E4_SYNC_TIMEOUT_MS {sync_timeout_ms}",
    ]
    doc = [
        "# W2.4 - Tham số barrier / notification",
        "",
        f"- Số task tham gia barrier: **{num_tasks}**.",
        f"- Bitmask đồng bộ (`xEventGroupSync`), mỗi task 1 bit: **0x{sync_bits_mask:02X}**.",
        f"- Timeout chờ barrier mỗi task: **{sync_timeout_ms} ms**.",
        "- Dùng tracer (W1.2) để đếm số lần gửi vs số sự kiện nhận được khi khảo sát bug",
        "  `eSetValueWithOverwrite`; chênh lệch > 0 là bằng chứng mất sự kiện.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W3.3 - per-ID assigned HardFault scenarios (self-triggered) + .noinit canary
# --------------------------------------------------------------------------
W3E3_FAULT_POOL = [
    "NULL_DEREF_READ",
    "NULL_DEREF_WRITE",
    "UNALIGNED_WORD_ACCESS",
    "DIV_BY_ZERO",
    "UNDEFINED_INSTRUCTION",
    "STACK_OVERFLOW_WRITE",
    "INVALID_PC_JUMP",
]


@exercise("w3e3")
def w3e3(r, mssv):
    num_assigned = r.randint(3, 5)
    assigned = r.sample(W3E3_FAULT_POOL, num_assigned)
    # 32-bit canary written into the .noinit post-mortem record right before
    # a deliberate fault is triggered; reading it back unchanged after reset
    # is how the student proves the .noinit region actually survived (a
    # power-on/brown-out reset would have cleared ordinary .bss/.data too,
    # but must NOT have touched .noinit for the post-mortem log to be valid).
    canary = int.from_bytes(
        hashlib.sha256(f"{mssv}:w3e3:canary".encode("utf-8")).digest()[:4], "big"
    )

    defines = [
        "/* HardFault post-mortem: fault scenarios this ID must self-trigger",
        "   and verify (one at a time, each a separate run) against your own",
        "   handler, plus a canary written into the .noinit record right",
        "   before each deliberate fault to prove .noinit survived reset. */",
        f"#define W3E3_NUM_ASSIGNED_FAULTS {num_assigned}",
        "static const char *const W3E3_ASSIGNED_FAULT_NAMES[W3E3_NUM_ASSIGNED_FAULTS] = {"
        + ", ".join(f'"{f}"' for f in assigned) + "};",
        f"#define W3E3_NOINIT_CANARY 0x{canary:08X}UL",
    ]
    doc = [
        "# W3.3 - Tham số HardFault post-mortem",
        "",
        f"- Các loại fault phải tự kích hoạt và kiểm chứng (mỗi loại 1 lần chạy riêng): "
        f"**{', '.join(assigned)}**.",
        f"- Giá trị canary ghi vào bản ghi `.noinit` ngay trước khi cố ý gây fault: "
        f"**0x{canary:08X}**.",
        "- Sau reset, nếu canary đọc lại KHÔNG khớp: `.noinit` đã bị khởi tạo lại (không",
        "  phải warm reset từ fault, hoặc linker script đặt sai section) — ghi rõ phát",
        "  hiện này vào REPORT.md, không được bỏ qua.",
        "- Với mỗi loại fault ở trên: log phải có CFSR/HFSR/BFAR hoặc MMFAR (tuỳ loại),",
        "  PC/LR từ stacked frame, tên task hiện tại, và dòng code xác định bằng",
        "  `arm-none-eabi-addr2line -e <elf> <PC>`.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W3.4 - per-ID GDB evidence/scan requirements for the black-box .elf
# (does NOT reveal the bug class - that defeats the black-box point).
# --------------------------------------------------------------------------
@exercise("w3e4")
def w3e4(r, mssv):
    min_evidence_commands = r.randint(4, 6)
    # A SRAM sub-window (within the F103C8's 20 KB, 0x20000000-0x20005000)
    # the student must dump first via GDB to locate candidate TCBs/lists
    # themselves, instead of being handed the addresses directly.
    scan_len = r.choice([0x400, 0x600, 0x800])
    max_start = 0x5000 - scan_len
    scan_start_offset = (r.randrange(0, max_start // 0x100 + 1)) * 0x100
    scan_start = 0x20000000 + scan_start_offset

    defines = [
        "/* Black-box GDB session requirements: minimum number of distinct",
        "   evidence commands/outputs to include in REPORT.md, and the SRAM",
        "   sub-window to dump first (x/...) when hunting for TCBs / list",
        "   structures - the exact addresses are yours to find, not given. */",
        f"#define W3E4_MIN_GDB_EVIDENCE_COMMANDS {min_evidence_commands}",
        f"#define W3E4_RAM_SCAN_START 0x{scan_start:08X}UL",
        f"#define W3E4_RAM_SCAN_LEN 0x{scan_len:X}UL",
    ]
    doc = [
        "# W3.4 - Tham số phiên GDB (debug hộp đen)",
        "",
        f"- Số lệnh GDB khác nhau tối thiểu phải có bằng chứng (output kèm giải thích) "
        f"trong REPORT.md: **{min_evidence_commands}**.",
        f"- Cửa sổ SRAM nên quét trước (vd. `x/{scan_len // 4}xw 0x{scan_start:08X}`) để "
        "tìm TCB / ready-list / delayed-list - **KHÔNG được cho sẵn địa chỉ chính xác**,",
        "  tự định vị bằng cách đối chiếu với bố cục `TCB_t`/`List_t` của FreeRTOS-Kernel.",
        "- Không được tiết lộ loại lỗi (deadlock / stack overflow / sai priority) ở đây;",
        "  tự kết luận bằng bằng chứng GDB, không đoán.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W4.1 - per-ID mixed-criticality design budgets (control loop jitter, RAM,
# CPU headroom, telemetry baud) - these are the numbers the design doc (and
# later W4.2/W4.3) must be checked against.
# --------------------------------------------------------------------------
W4E1_TELEMETRY_BAUD_CHOICES = [460800, 921600, 1000000]


@exercise("w4e1")
def w4e1(r, mssv):
    jitter_max_us = r.randint(20, 150)
    ram_budget_bytes = r.randint(14000, 17000)  # out of 20 KB total SRAM
    cpu_budget_percent_max = r.randint(60, 85)
    telemetry_baud = r.choice(W4E1_TELEMETRY_BAUD_CHOICES)

    defines = [
        "/* Mixed-criticality design budgets: max allowed jitter on the 1 kHz",
        "   TIM-triggered control loop, static-RAM ceiling for the whole",
        "   system (design must fit under this, not just the physical 20 KB),",
        "   worst-case total CPU budget, and the telemetry UART baud. */",
        f"#define W4E1_CONTROL_LOOP_HZ 1000UL",
        f"#define W4E1_IMU_RATE_HZ 500UL",
        f"#define W4E1_CONTROL_LOOP_JITTER_MAX_US {jitter_max_us}",
        f"#define W4E1_RAM_BUDGET_BYTES {ram_budget_bytes}UL",
        f"#define W4E1_CPU_BUDGET_PERCENT_MAX {cpu_budget_percent_max}",
        f"#define W4E1_TELEMETRY_BAUD_BPS {telemetry_baud}UL",
    ]
    doc = [
        "# W4.1 - Tham số ngân sách thiết kế mixed-criticality",
        "",
        "- Control loop kích bằng TIM: **1000 Hz** (chu kỳ 1 ms), IMU qua I2C1 (W2.1): **500 Hz**.",
        f"- Jitter tối đa cho phép của control loop: **{jitter_max_us} us** quanh mốc 1 ms.",
        f"- Ngân sách RAM tĩnh tối đa cho toàn hệ thống (thiết kế phải chứng minh nằm dưới "
        f"mức này, không chỉ dưới 20 KB vật lý): **{ram_budget_bytes} bytes**.",
        f"- Ngân sách CPU tối đa (tổng utilization mọi task, dự đoán bằng RTA): "
        f"**{cpu_budget_percent_max}%**.",
        f"- Baud telemetry UART DMA: **{telemetry_baud} bps**.",
        "- Design doc (RTA + bảng WCRT + ngân sách RAM/CPU + failure mode) phải được mentor",
        "  duyệt TRƯỚC khi viết code W4.2; không đạt ngân sách trên ở bước thiết kế thì phải",
        "  sửa thiết kế (đổi priority, chu kỳ, hoặc bớt việc trong ISR) trước khi code.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W4.2 - per-ID task set (priorities, stack budgets) for the mixed-criticality
# implementation; must match the W4.1 design doc once approved.
# --------------------------------------------------------------------------
W4E2_PRIORITY_POOL = [2, 3, 4, 5, 6, 7, 8]


@exercise("w4e2")
def w4e2(r, mssv):
    # 5 roles, ascending priority: idle-aux < shell < telemetry < imu < control.
    priorities = sorted(r.sample(W4E2_PRIORITY_POOL, 5))
    shell_cmd_timeout_ms = r.randint(50, 200)
    stack_margin_tolerance_words = r.randint(8, 32)

    defines = [
        "/* 5-role task priority set for the mixed-criticality system, ascending:",
        "   [idle_aux, shell, telemetry, imu, control]. Shell command parse",
        "   timeout and the allowed slack between your hand-budgeted stack size",
        "   and the measured high-water mark (uxTaskGetStackHighWaterMark). */",
        "#define W4E2_NUM_TASKS 5",
        "static const unsigned long W4E2_TASK_PRIORITY[W4E2_NUM_TASKS] = {"
        + ", ".join(str(p) for p in priorities) + "};",
        f"#define W4E2_SHELL_CMD_TIMEOUT_MS {shell_cmd_timeout_ms}",
        f"#define W4E2_STACK_MARGIN_TOLERANCE_WORDS {stack_margin_tolerance_words}",
    ]
    doc = [
        "# W4.2 - Tham số hiện thực (priority, timeout, ngân sách stack)",
        "",
        "| Vai trò | idle_aux | shell | telemetry | imu | control |",
        "|---|---|---|---|---|---|",
        "| Priority | " + " | ".join(str(p) for p in priorities) + " |",
        "",
        f"- Timeout parse lệnh shell: **{shell_cmd_timeout_ms} ms**.",
        f"- Sai số cho phép giữa ngân sách stack tự tính và "
        f"`uxTaskGetStackHighWaterMark()` đo được: **{stack_margin_tolerance_words} words**.",
        "- `control` là task ưu tiên cao nhất (chỉ số cuối); `idle_aux` thấp nhất (chỉ số đầu).",
        "- Priority này PHẢI khớp với RTA đã duyệt ở W4.1; nếu đổi, phải chạy lại RTA bằng tay",
        "  và giải thích trong REPORT.md.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W4.3 - per-ID verification bounds + assigned fault-injection scenarios.
# --------------------------------------------------------------------------
W4E3_FAULT_POOL = [
    "TASK_HANG",
    "UART_FLOOD",
    "SENSOR_UNPLUG",
    "CPU_OVERLOAD",
]


@exercise("w4e3")
def w4e3(r, mssv):
    wcrt_tolerance_percent = r.randint(10, 30)
    num_assigned_faults = r.randint(3, 4)
    assigned_faults = r.sample(W4E3_FAULT_POOL, num_assigned_faults)
    watchdog_recovery_timeout_ms = r.randint(200, 800)

    defines = [
        "/* Verification tolerances and the fault-injection scenarios this ID",
        "   must survive with controlled degradation (watchdog recovery,",
        "   logged reset reason) - see faults.md handed out live by the mentor",
        "   for exact trigger order, this only fixes the bound/pool. */",
        f"#define W4E3_WCRT_TOLERANCE_PERCENT {wcrt_tolerance_percent}",
        f"#define W4E3_NUM_ASSIGNED_FAULTS {num_assigned_faults}",
        "static const char *const W4E3_ASSIGNED_FAULT_NAMES[W4E3_NUM_ASSIGNED_FAULTS] = {"
        + ", ".join(f'"{f}"' for f in assigned_faults) + "};",
        f"#define W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS {watchdog_recovery_timeout_ms}",
    ]
    doc = [
        "# W4.3 - Tham số kiểm chứng + fault injection",
        "",
        f"- Sai số cho phép giữa WCRT dự đoán (RTA ở W4.1) và WCRT đo được "
        f"(`vTaskGetRunTimeStats`): **{wcrt_tolerance_percent}%**.",
        f"- Các kịch bản fault phải khảo sát và chứng minh suy giảm có kiểm soát: "
        f"**{', '.join(assigned_faults)}**.",
        f"- Timeout tối đa cho watchdog phục hồi hệ thống sau fault: "
        f"**{watchdog_recovery_timeout_ms} ms**.",
        "- Với mỗi fault ở trên: control loop không được trễ deadline, TRỪ KHI thiết kế",
        "  đã công bố trước (trong W4.1) là sẽ suy giảm có kiểm soát ở kịch bản đó -",
        "  nếu vậy phải ghi rõ lý do reset và bằng chứng log.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# W4.4 - defense session parameters (fixed by syllabus, not randomized beyond
# which subsystem the mentor's live parameter change targets).
# --------------------------------------------------------------------------
W4E4_MUTATION_TARGET_POOL = [
    "W4E1_CONTROL_LOOP_JITTER_MAX_US",
    "W4E2_TASK_PRIORITY",
    "W4E3_WATCHDOG_RECOVERY_TIMEOUT_MS",
]


@exercise("w4e4")
def w4e4(r, mssv):
    defense_duration_min = 15
    unknown_fault_count = 1
    # Not a secret the fault TYPE is unknown; this only picks which earlier
    # param the mentor will perturb live, so the student knows what kind of
    # re-prediction to rehearse (not which exact value).
    mutation_target = r.choice(W4E4_MUTATION_TARGET_POOL)

    defines = [
        "/* Defense session shape (fixed by SYLLABUS.md W4.4): duration and",
        "   number of previously-unseen faults the mentor injects live. The",
        "   mutation target only names WHICH earlier param family may be",
        "   changed on the spot, never the new value or the fault type. */",
        f"#define W4E4_DEFENSE_DURATION_MIN {defense_duration_min}",
        f"#define W4E4_UNKNOWN_FAULT_COUNT {unknown_fault_count}",
        f'#define W4E4_MUTATION_TARGET_HINT "{mutation_target}"',
    ]
    doc = [
        "# W4.4 - Tham số buổi bảo vệ",
        "",
        f"- Thời lượng: **{defense_duration_min} phút**.",
        f"- Số lỗi chưa biết trước mentor sẽ tiêm tại chỗ: **{unknown_fault_count}**.",
        f"- Nhóm tham số mentor có thể đổi tại chỗ trong buổi bảo vệ: "
        f"**{mutation_target}** (giá trị cụ thể KHÔNG được tiết lộ trước).",
        "- Phải dự đoán đúng HƯỚNG ảnh hưởng (tăng/giảm WCRT, jitter, RAM...) trước khi",
        "  mentor cho chạy lại, và chẩn đoán được lỗi tiêm vào bằng chính công cụ đo của mình.",
    ]
    return defines, doc


# --------------------------------------------------------------------------
# driver
# --------------------------------------------------------------------------
def render_header(key, mssv, defines):
    guard = f"{key.upper()}_PARAMS_H"
    lines = [
        "/* AUTO-GENERATED by tools/gen_params.py - do not edit by hand. */",
        f"/* MSSV: {mssv}  Exercise: {key} */",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
    ]
    lines += defines
    lines += ["", f"#endif /* {guard} */", ""]
    return "\n".join(lines)


def write_exercise(root, key, mssv):
    week, ex = key_to_path(key)
    out_dir = os.path.join(root, week, ex)
    inc_dir = os.path.join(out_dir, "include")
    os.makedirs(inc_dir, exist_ok=True)

    r = seeded_rng(mssv, key, "main")
    defines, doc = REGISTRY[key](r, mssv)

    with open(os.path.join(inc_dir, "params.h"), "w", encoding="utf-8") as f:
        f.write(render_header(key, mssv, defines))
    with open(os.path.join(out_dir, "PARAMS.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(doc) + "\n")
    return out_dir


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("mssv", nargs="?", help="student ID, e.g. 2112345")
    ap.add_argument("--only", help="comma-separated subset, e.g. w1e3,w3e1")
    ap.add_argument("--root", default=ROOT, help="repo root (default: course root)")
    ap.add_argument("--list", action="store_true", help="list registered exercises and exit")
    args = ap.parse_args(argv)

    if args.list:
        for k in sorted(REGISTRY):
            print(k)
        return 0

    if not args.mssv:
        ap.error("MSSV is required unless --list is given")

    keys = sorted(REGISTRY)
    if args.only:
        keys = [k.strip() for k in args.only.split(",") if k.strip()]
        for k in keys:
            if k not in REGISTRY:
                ap.error(f"unknown exercise key: {k} (known: {sorted(REGISTRY)})")

    for key in keys:
        out_dir = write_exercise(args.root, key, args.mssv)
        print(f"{key}: wrote {out_dir}/include/params.h + {out_dir}/PARAMS.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())
