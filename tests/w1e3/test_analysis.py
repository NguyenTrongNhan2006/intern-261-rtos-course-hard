import copy
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parent
code = root.parent.parent
spec = importlib.util.spec_from_file_location("v2", code / "tools/analyze_w1e3_v2.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
params = code / "week1/ex3/include/params.h"
periods, costs = m.parameters(params)
pending, arrivals, jobs = {}, {}, []
now, horizon = 0, 6000000
while now < horizon or pending:
    for i, t in enumerate(periods):
        if now < horizon and now % (t * 1000) == 0:
            assert i not in pending
            pending[i] = costs[i] // 72
            arrivals[i] = now
    if not pending:
        now = min((now // (t * 1000) + 1) * t * 1000 for t in periods)
        continue
    i = min(pending)
    nxt = min((now // (t * 1000) + 1) * t * 1000 for t in periods)
    end = min(now + pending[i], nxt)
    pending[i] -= end - now
    now = end
    if pending[i] == 0:
        jobs.append((i, arrivals.pop(i), now))
        del pending[i]
lines = ["# SYNTHETIC - NOT HARDWARE EVIDENCE", "# VERSION,2", "# IRQPROBE,10,12", "# HEAP,2000,1900"]
for i, c in enumerate(costs):
    lines.append(f"# CAL,{i+1},{c},{max(1,c//4)},{c},{c}")
    for task, release, finish in jobs:
        if task == i:
            seq = release // (periods[i] * 1000)
            lines.append(f"# JOB2,{i+1},{seq},{1440000+release*72},{1440000+finish*72},{c},0,0,0,0,0")
lines += ["# IRQ,0,1,100,100,100", "# IRQ,1,1,100,100,100", "# W1E3 DONE"]
generated = code / "build/w1e3-v2-host"
generated.mkdir(parents=True, exist_ok=True)
file = generated / "synthetic_v2.csv"
def run(candidate):
    file.write_text("\n".join(candidate) + "\n", encoding="utf-8")
    return m.analyze(file, params)
result = run(lines)
assert result["jobs"] == 205
assert [t["wcrt_cycles"] // 72 for t in result["tasks"]] == [9909,19971,51457,155928]
for kind in ["missing_job", "duplicate_cal", "bad_calibration", "incomplete", "bad_irq"]:
    bad = copy.copy(lines)
    if kind == "missing_job":
        del bad[next(i for i,l in enumerate(bad) if l.startswith("# JOB2,"))]
    elif kind == "duplicate_cal":
        bad.insert(3, next(l for l in bad if l.startswith("# CAL,")))
    elif kind == "bad_calibration":
        index = next(i for i,l in enumerate(bad) if l.startswith("# CAL,"))
        parts = bad[index].split(",");parts[-2]=str(costs[0]-1);bad[index]=",".join(parts)
    elif kind == "incomplete":
        bad.remove("# W1E3 DONE")
    else:
        bad[bad.index("# IRQ,0,1,100,100,100")] = "# IRQ,0,1,100,100,1000"
    try:
        run(bad)
    except ValueError:
        pass
    else:
        raise AssertionError(f"accepted {kind}")
run(lines)
print("PASS: 205-job V2 parse, ideal RTA, missing job, duplicate CAL, calibration bound, incomplete log, IRQ bounds")
