"""Validate V2 UART capture and report calibrated work + profiled IRQ time.

Only Python standard library. All arithmetic uses integer cycles at 72 MHz.
IRQ software brackets exclude hardware entry/exit and some wrapper edges.
PendSV brackets INCLUDE trace hooks: OUT->IN must not be added a second time.
"""
import argparse
import json
import math
import re
from pathlib import Path


def parameters(path):
    text = Path(path).read_text(encoding="utf-8-sig")
    def array(name):
        found = re.search(rf"{name}\s*\[[^]]+\]\s*=\s*\{{([^}}]+)\}}", text)
        if found is None:
            raise ValueError(f"missing {name}")
        return [int(x.strip()) for x in found[1].split(",")]
    periods = array("W1E3_PERIOD_MS")
    deadlines = array("W1E3_DEADLINE_MS")
    costs = array("W1E3_BUSY_WAIT_US")
    if len(periods) != 4 or not len(deadlines) == len(costs) == 4:
        raise ValueError("expected four tasks")
    if any(t <= 0 or d != t or c <= 0 for t, d, c in zip(periods, deadlines, costs)):
        raise ValueError("expected positive implicit-deadline tasks")
    return periods, [c * 72 for c in costs]


def response_times(periods, costs):
    result = {}
    order = sorted(range(4), key=lambda i: (periods[i], i))
    for rank, i in enumerate(order):
        current = costs[i]
        for _ in range(10000):
            nxt = costs[i] + sum(((current + periods[j] * 72000 - 1) //
                                 (periods[j] * 72000)) * costs[j] for j in order[:rank])
            if nxt == current or nxt > periods[i] * 72000:
                result[i + 1] = nxt
                break
            current = nxt
        else:
            raise ValueError("RTA did not converge")
    return result


def analyze(log, params):
    periods, costs = parameters(params)
    text = Path(log).read_text(encoding="utf-8-sig")
    if text.count("# VERSION,2") != 1 or text.count("# W1E3 DONE") != 1:
        raise ValueError("need exactly one complete V2 capture")
    calibration, jobs, irq = {}, {i: [] for i in range(1, 5)}, {}
    probe = heap = None
    for line in text.splitlines():
        if not line.startswith(("# CAL,", "# JOB2,", "# IRQ,", "# IRQPROBE,", "# HEAP,")):
            continue
        fields = line.split(",")
        values = list(map(int, fields[1:]))
        if any(x < 0 for x in values):
            raise ValueError("negative counter")
        if fields[0] == "# CAL":
            if len(values) != 5:
                raise ValueError("CAL field count")
            tag, target, iterations, minimum, maximum = values
            if tag not in jobs or tag in calibration or target != costs[tag - 1]:
                raise ValueError("CAL tag/target mismatch or duplicate")
            if iterations == 0 or not target <= minimum <= maximum <= target + 72:
                raise ValueError("calibration exceeds 1 us tolerance or is below target")
            calibration[tag] = dict(target=target, iterations=iterations, minimum=minimum, maximum=maximum)
        elif fields[0] == "# JOB2":
            if len(values) != 10:
                raise ValueError("JOB2 field count")
            tag, sequence, release, finish, wall, tick, pend, ntick, npend, resumes = values
            if tag not in jobs or finish <= release or finish >= 2**32:
                raise ValueError("invalid JOB2 tag/time or unsupported wrap")
            response = finish - release
            if wall > response or tick + pend >= response:
                raise ValueError("inconsistent work/IRQ intervals")
            jobs[tag].append(dict(sequence=sequence, release=release, finish=finish, wall=wall,
                                  tick=tick, pend=pend, tick_count=ntick, pend_count=npend, resumes=resumes))
        elif fields[0] == "# IRQ":
            if len(values) != 5:
                raise ValueError("IRQ field count")
            kind, count, minimum, maximum, total = values
            if kind not in (0, 1) or kind in irq or not count or not minimum <= maximum:
                raise ValueError("invalid IRQ counters")
            if not minimum * count <= total <= maximum * count:
                raise ValueError("IRQ sum outside bounds")
            irq[kind] = dict(count=count, minimum=minimum, maximum=maximum, total=total)
        elif fields[0] == "# IRQPROBE":
            if len(values) != 2 or values[0] > values[1]:
                raise ValueError("invalid IRQPROBE")
            probe = values
        else:
            if len(values) != 2 or values[1] > values[0]:
                raise ValueError("invalid HEAP")
            heap = values
    if len(calibration) != 4 or len(irq) != 2 or probe is None or heap is None:
        raise ValueError("missing calibration, IRQ or heap metadata")
    h = math.lcm(*periods)
    releases = {}
    all_jobs = []
    for tag, rows in jobs.items():
        expected = 5 * h // periods[tag - 1]
        rows.sort(key=lambda r: r["sequence"])
        if [r["sequence"] for r in rows] != list(range(expected)):
            raise ValueError(f"tag {tag}: missing or duplicate job")
        for row in rows:
            nominal_tick = row["sequence"] * periods[tag - 1]
            previous = releases.setdefault(nominal_tick, row["release"])
            if previous != row["release"]:
                raise ValueError("tasks at same tick have different release timestamps")
            if row["wall"] + 72 < calibration[tag]["minimum"]:
                raise ValueError("work wall time below calibrated minimum")
            all_jobs.append(dict(tag=tag, **row))
    epoch = releases[0]
    errors = [actual - (epoch + tick * 72000) for tick, actual in sorted(releases.items())]
    if any(b <= a for a, b in zip([v for _, v in sorted(releases.items())],
                                 [v for _, v in sorted(releases.items())][1:])):
        raise ValueError("release time not increasing")
    ideal = response_times(periods, costs)
    calibrated_max = response_times(periods, [calibration[i]["maximum"] for i in range(1, 5)])
    order = sorted(range(1, 5), key=lambda t: (periods[t - 1], t))
    summaries = []
    for tag in range(1, 5):
        rows = jobs[tag]
        worst = max(rows, key=lambda r: r["finish"] - r["release"])
        r, f = worst["release"], worst["finish"]
        response = f - r
        summary = dict(tag=tag, jobs=len(rows), rta_cycles=ideal[tag],
                       calibrated_max_rta_cycles=calibrated_max[tag], wcrt_cycles=response,
                       delta_cycles=response - ideal[tag], worst_sequence=worst["sequence"],
                       misses=sum(x["finish"] - x["release"] > periods[tag - 1] * 72000 for x in rows),
                       tick_cycles=worst["tick"], pendsv_cycles=worst["pend"],
                       tick_count=worst["tick_count"], pendsv_count=worst["pend_count"],
                       resumes=worst["resumes"])
        higher = set(order[:order.index(tag) + 1])
        carry = [x for x in all_jobs if x["tag"] in higher and x["release"] < r < x["finish"]]
        selected = [x for x in all_jobs if x["tag"] in higher and r <= x["release"] and x["finish"] <= f]
        # Only apply this simple residual to windows of complete jobs; do
        # not silently subtract full C for a partial carry-in job.
        if not carry:
            cal_min = sum(calibration[x["tag"]]["minimum"] for x in selected)
            cal_max = sum(calibration[x["tag"]]["maximum"] for x in selected)
            summary["complete_work_counts"] = [sum(x["tag"] == t for x in selected) for t in range(1, 5)]
            summary["residual_low_cycles"] = response - worst["tick"] - worst["pend"] - cal_max
            summary["residual_high_cycles"] = response - worst["tick"] - worst["pend"] - cal_min
        else:
            summary["residual_note"] = "carry-in: no full-job calibration subtraction"
        summaries.append(summary)
    return dict(version=2, cycle_hz=72000000, hyperperiod_ms=h, jobs=len(all_jobs),
                calibration=calibration, irq=irq, irq_probe_cycles=probe, heap_bytes=heap,
                release_grid_error_cycles=[min(errors), max(errors)], tasks=summaries,
                limitations="Calibration is isolated empirical timing. IRQ brackets omit hardware entry/exit and wrapper edges; PendSV includes tracer. Finite observed maxima.")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("log")
    ap.add_argument("--params", default="week1/ex3/include/params.h")
    ap.add_argument("--json-out")
    args = ap.parse_args()
    result = analyze(args.log, args.params)
    print(f"V2: {result['jobs']} jobs; H={result['hyperperiod_ms']} ms; calibration <= 1 us over target")
    for row in result["tasks"]:
        print(f"tau{row['tag']-1}: n={row['jobs']} RTA={row['rta_cycles']/72000:.6f} ms "
              f"WCRT={row['wcrt_cycles']/72000:.6f} ms misses={row['misses']} "
              f"tick={row['tick_cycles']/72:.3f} us PendSV={row['pendsv_cycles']/72:.3f} us")
        if "residual_low_cycles" in row:
            print(f"  residual outside measured IRQ/calibration: "
                  f"{row['residual_low_cycles']/72:.3f}..{row['residual_high_cycles']/72:.3f} us")
        else:
            print("  " + row["residual_note"])
    print("Do not add OUT->IN to PendSV again; profiling has observer overhead.")
    if args.json_out:
        Path(args.json_out).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
