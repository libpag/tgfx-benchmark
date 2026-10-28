import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
import bench_base, bench_shadow

results_by_label = {}
runs_root = os.path.join(bench_base.PROJECT_ROOT, "runs")
for label in sorted(os.listdir(runs_root)):
    label_dir = os.path.join(runs_root, label)
    if not os.path.isdir(label_dir):
        continue
    per_bench = {}
    for name in bench_shadow.BENCH_NAMES:
        bench_dir = os.path.join(label_dir, name)
        if not os.path.isdir(bench_dir):
            continue
        results = []
        logs = sorted((f for f in os.listdir(bench_dir) if f.startswith("run") and f.endswith(".log")),
                      key=lambda f: int(re.search(r"run(\d+)", f).group(1)))
        for f in logs:
            text = open(os.path.join(bench_dir, f)).read()
            samples = bench_base.PERF_RE.findall(text)
            if not samples:
                results.append(bench_base.RunResult(failed=True, reason="no PERF line in log"))
                continue
            fps_values = [float(m[1]) for m in samples]
            count = int(samples[-1][2])
            results.append(bench_base.RunResult(fps=sum(fps_values) / len(fps_values), draw_count=count))
        per_bench[name] = results
    if per_bench:
        results_by_label[label] = per_bench

for label, benches in results_by_label.items():
    for n, v in benches.items():
        print(label, n, [repr(r) for r in v])
bench_shadow.write_report(results_by_label, bench_shadow.BENCH_NAMES)
