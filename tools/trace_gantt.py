#!/usr/bin/env python3
"""W1.2 host-side Gantt renderer - stdlib only, no external dependencies.

Reads a trace log dumped over UART by the student's own trace hook
(traceTASK_SWITCHED_IN/OUT) and renders:
  - a text (ASCII) Gantt chart on stdout
  - an optional self-contained SVG file (--svg out.svg)
  - context-switch timing stats (min/max/mean/stdev), the W1.2 Output item

Expected log format (one event per line, produced by the student's own UART
dump tool - this script only defines the host-side contract):
    <cyccnt_cycles>,<task_tag>,<event>
where <event> is exactly "IN" or "OUT". Blank lines and lines starting with
'#' are ignored. CYCCNT is assumed monotonic non-wrapping across the capture
(wrap handling is the student's problem in their dump tool, not this script's).

Usage:
    python tools/trace_gantt.py trace.csv --freq-hz 72000000
    python tools/trace_gantt.py trace.csv --svg gantt.svg --params-h week1/ex2/include/params.h
"""
import argparse
import re
import statistics
import sys


def parse_log(path):
    events = []  # (cycles:int, tag:str, kind:str)
    warnings = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if len(parts) != 3:
                warnings.append(f"line {lineno}: expected 3 fields, got {line!r}")
                continue
            cyc_s, tag, kind = (p.strip() for p in parts)
            kind = kind.upper()
            if kind not in ("IN", "OUT"):
                warnings.append(f"line {lineno}: bad event {kind!r}")
                continue
            try:
                cyc = int(cyc_s)
            except ValueError:
                warnings.append(f"line {lineno}: bad cycle count {cyc_s!r}")
                continue
            events.append((cyc, tag, kind))
    events.sort(key=lambda e: e[0])
    return events, warnings


def build_intervals(events):
    """Turn IN/OUT events into (tag, start, end) intervals and a list of
    context-switch gaps (end of one OUT -> next IN), skipping unmatched
    IN/OUT pairs instead of crashing on a malformed capture."""
    intervals = []
    switches = []  # cycles between consecutive OUT -> IN
    open_tag = None
    open_start = None
    last_out = None
    anomalies = []

    for cyc, tag, kind in events:
        if kind == "IN":
            if open_tag is not None:
                anomalies.append(f"IN for {tag}@{cyc} while {open_tag} still open")
            else:
                if last_out is not None:
                    switches.append(cyc - last_out)
                open_tag, open_start = tag, cyc
        else:  # OUT
            if open_tag is None or open_tag != tag:
                anomalies.append(f"OUT for {tag}@{cyc} without matching IN")
                continue
            intervals.append((tag, open_start, cyc))
            open_tag, open_start = None, None
            last_out = cyc

    if open_tag is not None:
        anomalies.append(f"trace ended with {open_tag} still switched IN")
    return intervals, switches, anomalies


def ascii_gantt(intervals, width=100):
    if not intervals:
        return "(no intervals to draw)"
    tags = sorted({t for t, _, _ in intervals}, key=str)
    t0 = min(s for _, s, _ in intervals)
    t1 = max(e for _, _, e in intervals)
    span = max(1, t1 - t0)

    def col(c):
        return min(width - 1, int((c - t0) / span * width))

    rows = []
    for tag in tags:
        row = [" "] * width
        for t, s, e in intervals:
            if t != tag:
                continue
            a, b = col(s), max(col(s), col(e))
            for i in range(a, b + 1):
                row[i] = "#"
        rows.append(f"{tag:>8} |" + "".join(row))
    header = " " * 9 + "+" + "-" * width
    return "\n".join([header] + rows + [header])


def write_svg(path, intervals, freq_hz):
    if not intervals:
        return
    tags = sorted({t for t, _, _ in intervals}, key=str)
    row_h, left_pad, top_pad = 24, 90, 20
    t0 = min(s for _, s, _ in intervals)
    t1 = max(e for _, _, e in intervals)
    span = max(1, t1 - t0)
    width_px = 900
    height_px = top_pad * 2 + row_h * len(tags)

    def x(c):
        return left_pad + (c - t0) / span * (width_px - left_pad - 10)

    colors = ["#4c78a8", "#f58518", "#54a24b", "#e45756", "#72b7b2", "#b279a2"]
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width_px}" '
        f'height="{height_px}" font-family="monospace" font-size="12">',
        f'<rect width="100%" height="100%" fill="white"/>',
    ]
    for row_i, tag in enumerate(tags):
        y = top_pad + row_i * row_h
        color = colors[row_i % len(colors)]
        parts.append(f'<text x="5" y="{y + row_h / 2 + 4}">{tag}</text>')
        for t, s, e in intervals:
            if t != tag:
                continue
            x0, x1 = x(s), max(x(s) + 1, x(e))
            parts.append(
                f'<rect x="{x0:.1f}" y="{y + 2}" width="{x1 - x0:.1f}" '
                f'height="{row_h - 4}" fill="{color}"/>'
            )
    dur_ms = span / freq_hz * 1000
    parts.append(f'<text x="{left_pad}" y="{height_px - 4}">span = {dur_ms:.2f} ms</text>')
    parts.append("</svg>")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(parts))


def load_define(path, name):
    """Best-effort pull of a #define NUMBER from a generated params.h."""
    try:
        with open(path, encoding="utf-8") as f:
            text = f.read()
    except OSError:
        return None
    m = re.search(rf"#define\s+{re.escape(name)}\s+([0-9]+)", text)
    return int(m.group(1)) if m else None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("log", help="CSV trace log: cycles,task_tag,IN|OUT")
    ap.add_argument("--freq-hz", type=float, default=72_000_000, help="DWT CYCCNT clock (default 72 MHz)")
    ap.add_argument("--svg", help="also write a self-contained SVG Gantt chart here")
    ap.add_argument("--width", type=int, default=100, help="ASCII chart width in columns")
    ap.add_argument("--params-h", help="optional params.h to read W1E2_CTXSW_JITTER_MAX_CYCLES_O2 from")
    args = ap.parse_args(argv)

    events, warnings = parse_log(args.log)
    for w in warnings:
        print(f"warning: {w}", file=sys.stderr)
    if not events:
        print("no valid events parsed; nothing to draw", file=sys.stderr)
        return 1

    intervals, switches, anomalies = build_intervals(events)
    for a in anomalies:
        print(f"anomaly: {a}", file=sys.stderr)

    print(ascii_gantt(intervals, width=args.width))
    print()

    if switches:
        ns = [c / args.freq_hz * 1e9 for c in switches]
        print("Context-switch timing (OUT -> next IN):")
        print(f"  n       = {len(ns)}")
        print(f"  min     = {min(switches)} cyc ({min(ns):.1f} ns)")
        print(f"  max     = {max(switches)} cyc ({max(ns):.1f} ns)")
        print(f"  mean    = {statistics.mean(switches):.1f} cyc ({statistics.mean(ns):.1f} ns)")
        if len(switches) > 1:
            print(f"  stdev   = {statistics.stdev(switches):.1f} cyc")

        if args.params_h:
            bound = load_define(args.params_h, "W1E2_CTXSW_JITTER_MAX_CYCLES_O2")
            if bound is not None:
                jitter = max(switches) - min(switches)
                verdict = "PASS" if jitter <= bound else "FAIL"
                print(f"  jitter  = {jitter} cyc vs bound {bound} cyc -> {verdict}")
    else:
        print("no OUT->IN pairs found; cannot compute context-switch stats")

    if args.svg:
        write_svg(args.svg, intervals, args.freq_hz)
        print(f"\nwrote {args.svg}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
