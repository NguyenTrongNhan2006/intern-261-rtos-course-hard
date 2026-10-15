#!/usr/bin/env python3
"""Correlate ordered UART events with an 8-channel sigrok v2 session.

Stdlib only. Does not sort UART timestamps, rewrite evidence, fit clock
scale, or treat offset-fitted residuals as calibrated instrument accuracy.
The first-edge maximum is the conservative pass/fail criterion. Mean fits
are reported separately for all events and for PendSV events (excluding
only the first, thread-mode IN). Python integers accept the firmware's
unwrapped 64-bit cycles; raw 32-bit wrap is intentionally rejected here.
"""
import argparse
import configparser
import json
import math
from pathlib import Path
import re
import statistics
import sys
import zipfile


def read_log(path):
    captures, records = [], []
    for number, raw in enumerate(Path(path).read_text(encoding="utf-8-sig").splitlines(), 1):
        line = raw.strip()
        if not line:
            continue
        if line == "# W1E2 SCHEDULER TRACE":
            if records:
                raise ValueError(f"line {number}: reset before capture ended")
            continue
        if line.startswith("# overflow=") and int(line.split("=", 1)[1]) != 0:
            raise ValueError(f"line {number}: firmware reported overflow")
        if line == "# CAPTURE DONE":
            if not records:
                raise ValueError(f"line {number}: empty capture")
            validate_records(records)
            captures.append(records)
            records = []
            continue
        if line.startswith("#"):
            continue
        parts = line.split(",")
        if len(parts) != 3:
            raise ValueError(f"line {number}: expected cycles,tag,IN|OUT")
        cycles, tag = int(parts[0]), int(parts[1])
        kind = parts[2].strip()
        if cycles < 0 or not 0 <= tag <= 255 or kind not in ("IN", "OUT"):
            raise ValueError(f"line {number}: invalid event")
        records.append((cycles, tag, kind))
    if records:
        validate_records(records)
        captures.append(records)
    if not captures:
        raise ValueError("no captures")
    return captures


def validate_records(records):
    if not records or len(records) % 2:
        raise ValueError("capture must contain complete IN/OUT pairs")
    previous = -1
    for index, (cycles, tag, kind) in enumerate(records):
        if cycles <= previous:
            raise ValueError("cycles must increase in input order; unwrap in firmware dump")
        if kind != ("IN" if index % 2 == 0 else "OUT"):
            raise ValueError("invalid IN/OUT ordering")
        if index % 2 and tag != records[index - 1][1]:
            raise ValueError("IN/OUT task tags differ")
        previous = cycles


def read_session(path, channels=(0, 1, 2)):
    if len(set(channels)) != len(channels) or not channels or any(c < 0 or c > 7 for c in channels):
        raise ValueError("channels must be distinct bit indices 0..7 in tag order")
    with zipfile.ZipFile(path) as archive:
        if archive.read("version").strip() != b"2":
            raise ValueError("only sigrok v2 is supported")
        metadata = configparser.ConfigParser()
        metadata.read_string(archive.read("metadata").decode("utf-8"))
        device = metadata["device 1"]
        if device.getint("unitsize") != 1:
            raise ValueError("only one-byte digital samples are supported")
        rate = re.fullmatch(r"\s*([\d.]+)\s*([kMG]?)Hz\s*", device["samplerate"])
        if rate is None:
            raise ValueError("unsupported samplerate")
        samplerate = float(rate[1]) * {"": 1, "k": 1e3, "M": 1e6, "G": 1e9}[rate[2]]
        if not math.isfinite(samplerate) or samplerate <= 0:
            raise ValueError("samplerate must be positive")
        base = device.get("capturefile", "logic-1")
        chunks = []
        for name in archive.namelist():
            match = re.fullmatch(re.escape(base) + r"-(\d+)", name)
            if match:
                chunks.append((int(match[1]), name))
        if chunks:
            chunks.sort()
            if [n for n, _ in chunks] != list(range(1, len(chunks) + 1)):
                raise ValueError("missing or duplicate logic chunk")
            data = b"".join(archive.read(name) for _, name in chunks)
        else:
            data = archive.read(base)
    if not data:
        raise ValueError("empty logic data")
    mask = sum(1 << channel for channel in channels)
    previous, edges = data[0] & mask, []
    for sample, value in enumerate(data[1:], 1):
        current = value & mask
        changed = current ^ previous
        if changed:
            for tag, channel in enumerate(channels):
                bit = 1 << channel
                if changed & bit:
                    edges.append((sample, tag, "IN" if current & bit else "OUT"))
        previous = current
    return samplerate, len(data), edges


def summarize(values):
    return {"mean_us": statistics.mean(values), "sd_us": statistics.pstdev(values),
            "min_us": min(values), "max_us": max(values),
            "max_abs_us": max(map(abs, values))}


def next_match(edges, records, cursor):
    signature = [(tag, kind) for _, tag, kind in records]
    count = len(records)
    for index in range(cursor, len(edges) - count + 1):
        if (edges[index][1], edges[index][2]) != signature[0]:
            continue
        selected = edges[index:index + count]
        if [(tag, kind) for _, tag, kind in selected] == signature:
            return index, selected
    return None


def compare(records, selected, samplerate, freq_hz):
    first = [(sample - selected[0][0]) / samplerate * 1e6 -
             (cycle - records[0][0]) / freq_hz * 1e6
             for (sample, _, _), (cycle, _, _) in zip(selected, records)]
    mean_all = statistics.mean(first)
    # Only event zero is thread-mode for this W1E2 workload.
    isr = first[1:]
    mean_isr = statistics.mean(isr)
    worst = max(range(len(first)), key=lambda i: abs(first[i]))
    return {"origin_us": selected[0][0] / samplerate * 1e6,
            "events": len(records), "first_edge": summarize(first),
            "mean_all": summarize([v - mean_all for v in first]),
            "mean_pendsv": summarize([v - mean_isr for v in isr]),
            "pendsv_offset_relative_first_us": mean_isr,
            "first_thread_residual_after_pendsv_fit_us": -mean_isr,
            "worst_first_event": worst + 1}


def switch_stats(records):
    groups = {}
    for a, b in zip(records, records[1:]):
        if a[2] == "OUT" and b[2] == "IN":
            groups.setdefault((a[1], b[1]), []).append(b[0] - a[0])
    gaps = [value for values in groups.values() for value in values]
    return {"n": len(gaps), "min": min(gaps) if gaps else None,
            "max": max(gaps) if gaps else None,
            "mean": statistics.mean(gaps) if gaps else None,
            "jitter": max(gaps) - min(gaps) if gaps else None,
            "directions": [{"from": a, "to": b, "n": len(values),
                            "min": min(values), "max": max(values)}
                           for (a, b), values in sorted(groups.items())]}


def analyze(session, log, freq_hz=72e6, tolerance_us=16, channels=(0, 1, 2)):
    if not math.isfinite(freq_hz) or freq_hz <= 0:
        raise ValueError("freq-hz must be positive")
    if not math.isfinite(tolerance_us) or tolerance_us < 0:
        raise ValueError("tolerance-us must be nonnegative")
    captures = read_log(log)
    if any(tag >= len(channels) for capture in captures for _, tag, _ in capture):
        raise ValueError("UART tag has no mapped logic channel")
    samplerate, samples, edges = read_session(session, channels)
    cursor, matches = 0, []
    if len(captures) == 1:
        while True:
            found = next_match(edges, captures[0], cursor)
            if found is None:
                break
            start, selected = found
            matches.append(compare(captures[0], selected, samplerate, freq_hz))
            cursor = start + len(selected)
    else:
        for capture in captures:
            found = next_match(edges, capture, cursor)
            if found is None:
                raise ValueError("no complete GPIO match for a UART capture")
            start, selected = found
            matches.append(compare(capture, selected, samplerate, freq_hz))
            cursor = start + len(selected)
    if not matches:
        raise ValueError("no complete GPIO match")
    return {"samplerate_hz": samplerate, "samples": samples,
            "uart_captures": len(captures),
            "uart_captures_identical": all(c == captures[0] for c in captures),
            "matches": matches, "switches": switch_stats(captures[0]),
            "tolerance_us": tolerance_us,
            "pass": all(m["first_edge"]["max_abs_us"] <= tolerance_us for m in matches)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("session", help="sigrok v2 .sr file")
    parser.add_argument("log", help="ordered UART CSV, one or more captures")
    parser.add_argument("--freq-hz", type=float, default=72e6)
    parser.add_argument("--tolerance-us", type=float, default=16)
    parser.add_argument("--channels", default="0,1,2", help="GPIO channel bits in task-tag order")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = analyze(args.session, args.log, args.freq_hz, args.tolerance_us,
                         tuple(int(c) for c in args.channels.split(",")))
    except (ValueError, OSError, KeyError, zipfile.BadZipFile, configparser.Error) as error:
        print(f"trace_correlate: {error}", file=sys.stderr)
        return 2
    if args.json:
        print(json.dumps(result, indent=2, allow_nan=False))
    else:
        print(f"Sample rate: {result['samplerate_hz']:g} Hz; samples: {result['samples']}")
        print(f"UART captures: {result['uart_captures']}; identical: {result['uart_captures_identical']}")
        print("capture events origin_us first_max_us mean_all_max_us PendSV_mean_us PendSV_sd_us PendSV_fit_max_us")
        for index, match in enumerate(result["matches"], 1):
            print(f"{index:7} {match['events']:6} {match['origin_us']:9.0f} "
                  f"{match['first_edge']['max_abs_us']:12.6f} {match['mean_all']['max_abs_us']:15.6f} "
                  f"{match['pendsv_offset_relative_first_us']:14.6f} "
                  f"{match['mean_pendsv']['sd_us']:12.6f} {match['mean_pendsv']['max_abs_us']:17.6f}")
        print("OUT -> next IN:", result["switches"])
        print("First-edge bound:", "PASS" if result["pass"] else "FAIL")
        print("Mean fits remove a constant offset; they do not calibrate either clock.")
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
