#!/usr/bin/env python3
"""Audit one complete buggy/fixed W2E4 log and extract report evidence.

Run from the repository root:
  python tools/analyze_w2e4.py
  python tools/analyze_w2e4.py --figures  # additionally requires matplotlib

Raw logs are never rewritten. Notification EV records and task-switch Gantt
records are separate streams; a full EV stream need not have a full Gantt.
"""
import argparse
import csv
import json
import re
import statistics
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from trace_gantt import build_intervals, parse_log, write_svg


def require(condition, message):
    if not condition:
        raise ValueError(message)


def stats(values):
    return {'min': min(values), 'max': max(values),
            'mean': statistics.mean(values), 'median': statistics.median(values)}


def audit(directory, name, fixed):
    source = directory / f'{name}.log.txt'
    lines = source.read_text(encoding='utf-8-sig').splitlines()
    require(sum(s.startswith('# USE_FIXED=') for s in lines) == 1
            and lines.count('# END') == 1, f'{source}: need one complete run')
    meta = {}
    events = []
    gantt = []
    for line in lines:
        if match := re.fullmatch(r'# ([A-Za-z_]+)=(.*)', line):
            key, value = match.groups()
            require(key not in meta, f'{source}: duplicate metadata {key}')
            meta[key] = value
        elif match := re.fullmatch(r'# EV,(\d+),([SR]),(\d+)', line):
            cycles, kind, value = match.groups()
            events.append((int(cycles), kind, int(value)))
        elif re.fullmatch(r'\d+,\d+,(IN|OUT)', line):
            gantt.append(line)
        elif line.startswith('# EV,'):
            raise ValueError(f'{source}: malformed event: {line}')
    number = int(meta['SEND_COUNT'])
    require(int(meta['USE_FIXED']) == fixed and number > 0 and number % 2 == 0,
            f'{source}: wrong variant/count')
    require(meta['BARRIER'] == 'PASS' and meta['TRACE_DROPPED'] == '0'
            and meta['overflow'] == '0', f'{source}: failed barrier/trace')
    require(all(a[0] < b[0] for a, b in zip(events, events[1:])),
            f'{source}: timestamps are not increasing; check reset/wrap')
    sends = [e for e in events if e[1] == 'S']
    receives = [e for e in events if e[1] == 'R']
    require([e[2] for e in sends] == list(range(1, number + 1)),
            f'{source}: send sequence incomplete')
    require(len(events) == number * 3 // 2, f'{source}: incomplete EV stream')
    for i in range(number // 2):
        group = events[i * 3:i * 3 + 3]
        require([e[1] for e in group] == ['S', 'S', 'R'],
                f'{source}: unexpected scheduling at pair {i + 1}')
        require(group[0][2] == i * 2 + 1 and group[1][2] == i * 2 + 2,
                f'{source}: wrong sender pair {i + 1}')
        require(group[2][2] == (2 if fixed else i * 2 + 2),
                f'{source}: wrong receive value at pair {i + 1}')
    received_events = sum(e[2] for e in receives) if fixed else len(receives)
    lost = number - received_events
    require(int(meta['SENT']) == number
            and int(meta['RECEIVED_EVENTS']) == received_events
            and int(meta.get('LOST_EVENTS', '0')) == lost,
            f'{source}: counter/EV disagreement')
    require(meta['STATUS'] == ('BALANCED' if fixed else 'LOSS_CONFIRMED'),
            f'{source}: unexpected status')
    require(len(gantt) == int(meta['GANTT_RECORDS']),
            f'{source}: Gantt count mismatch')
    trace_path = directory / f'{name}.gantt.trace.csv'
    trace_path.write_text('\n'.join(gantt) + '\n', encoding='ascii')
    parsed, warnings = parse_log(trace_path)
    intervals, gaps, anomalies = build_intervals(parsed)
    require(not warnings and not anomalies, f'{source}: Gantt pairing error')
    write_svg(directory / f'{name}.gantt.svg', intervals, int(meta['CPU_HZ']))
    with (directory / f'{name}.events.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['cycles', 'kind', 'value'])
        writer.writerows(events)
    first_sends = sends[::2]
    details = {
        'variant': name, 'metadata': meta, 'send_records': len(sends),
        'receive_calls': len(receives), 'received_events_from_trace': received_events,
        'lost_events_from_trace': lost, 'first_pair': events[:3],
        'pair_period_cycles': stats([b[0] - a[0] for a, b in zip(first_sends, first_sends[1:])]),
        'send_marker_gap_cycles': stats([events[i + 1][0] - events[i][0]
                                        for i in range(0, len(events), 3)]),
        'second_send_to_receive_marker_cycles': stats([events[i + 2][0] - events[i + 1][0]
                                                      for i in range(0, len(events), 3)]),
        'gantt_first_cycles': parsed[0][0], 'gantt_last_cycles': parsed[-1][0],
        'last_event_cycles': events[-1][0],
        'gantt_gap_cycles': stats(gaps),
    }
    print(f'{name}: PASS; S={len(sends)}, R calls={len(receives)}, '
          f'received events={received_events}, lost={lost}, EV dropped=0; '
          f'Gantt={len(gantt)} paired records')
    return details, events


def figures(directory, runs):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10})
    fig, axes = plt.subplots(1, 2, figsize=(12, 3.8))
    for ax, (details, events) in zip(axes, runs):
        origin = events[0][0]
        for i, (cycle, kind, value) in enumerate(events[:3]):
            x = (cycle - origin) / 72
            y = 1 if kind == 'S' else 0
            ax.scatter([x], [y], color='#4c78a8' if kind == 'S' else '#e15759', s=55)
            ax.annotate(f'{kind}={value}\n+{x:.3f} us', (x, y),
                        xytext=(0, 14 if i != 1 else -42),
                        textcoords='offset points', ha='center', fontsize=9)
        end = (events[2][0] - origin) / 72
        ax.set_xlim(-5, end + 8)
        ax.set_ylim(-.4, 1.55)
        ax.set_yticks([0, 1], ['Receiver', 'Sender'])
        ax.set_xlabel('Time from first S marker (us), 72 MHz')
        ax.set_title(details['variant'] + (': R is a count' if details['variant']=='fixed' else ': R is the latest sequence'))
        ax.grid(axis='x', alpha=.2)
    fig.suptitle('First pair: Sender runs twice before Receiver runs')
    fig.tight_layout()
    save_figure(fig, directory / 'first_pair.svg')
    fig, axes = plt.subplots(1, 2, figsize=(12, 3.8))
    for ax, (details, events) in zip(axes, runs):
        t, sent, received = [0.], [0], [0]
        ns, nr = 0, 0
        for cycle, kind, value in events:
            ns += kind == 'S'
            if kind == 'R':
                nr += value if details['variant'] == 'fixed' else 1
            t.append(cycle / 72e6)
            sent.append(ns); received.append(nr)
        ax.step(t, sent, where='post', lw=2.4, alpha=.6, label='Sent', color='#4c78a8')
        ax.step(t, received, where='post', lw=1.3, ls='--', label='Received events', color='#e15759')
        ax.set_title(f'{details["variant"]}: sent {ns}, received {nr}, lost {ns-nr}')
        ax.set_xlabel('DWT time (s)'); ax.set_ylabel('Cumulative events')
        ax.grid(alpha=.2); ax.legend()
    fig.tight_layout()
    save_figure(fig, directory / 'event_counts.svg')


def save_figure(fig, path):
    import matplotlib.pyplot as plt
    fig.savefig(path, bbox_inches='tight')
    path.write_text('\n'.join(s.rstrip() for s in path.read_text().splitlines()) + '\n',
                    encoding='utf-8')
    preview = Path('build/w2e4-review') / (path.stem + '.png')
    if preview.parent.exists():
        fig.savefig(preview, dpi=130, bbox_inches='tight')
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path('week2/ex4/measurements'))
    parser.add_argument('--figures', action='store_true')
    args = parser.parse_args()
    runs = [audit(args.directory, 'buggy', 0), audit(args.directory, 'fixed', 1)]
    details = [r[0] for r in runs]
    (args.directory / 'audit.json').write_text(json.dumps(details, indent=2) + '\n', encoding='utf-8')
    keys = ['USE_FIXED','SENT','RECEIVED_EVENTS','TRACE_DROPPED','STATUS',
            'BARRIER_RELEASE_CYCLES','BINARY_PAIR_CYCLES','FIXED_NOTIFY_PAIR_CYCLES',
            'RAM_SYNC_BINARY','RAM_SYNC_NOTIFY','GANTT_RECORDS']
    with (args.directory/'summary.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.writer(f)
        writer.writerow(['variant',*keys,'RECEIVE_CALLS','LOST_EVENTS'])
        for d in details:
            writer.writerow([d['variant'],*[d['metadata'][k] for k in keys],
                             d['receive_calls'],d['lost_events_from_trace']])
    if args.figures:
        figures(args.directory, runs)


if __name__ == '__main__':
    main()
