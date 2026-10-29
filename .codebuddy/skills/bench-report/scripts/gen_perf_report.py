#!/usr/bin/env python3
"""
Regenerates a scenario report from archived run logs (runs/<label>/<bench>/run<N>.log)
without re-running the benchmark.

Usage:
  python3 gen_perf_report.py <scenario script path>

Example:
  python3 gen_perf_report.py test/shadow/bench_shadow.py

The scenario script must expose BENCH_NAMES and write_report(results_by_label,
rt_by_label, bench_names). Paths are resolved against the repo root.
"""

import importlib.util
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bench_base


def load_scenario(path):
    spec = importlib.util.spec_from_file_location("bench_scenario", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    scenario_path = os.path.normpath(os.path.join(bench_base.PROJECT_ROOT, sys.argv[1]))
    if not os.path.isfile(scenario_path):
        print(f"scenario script not found: {scenario_path}", file=sys.stderr)
        sys.exit(2)
    scenario = load_scenario(scenario_path)

    results_by_label = {}
    rt_by_label = {}
    runs_root = os.path.join(bench_base.PROJECT_ROOT, "runs")
    for label in sorted(os.listdir(runs_root)):
        label_dir = os.path.join(runs_root, label)
        if not os.path.isdir(label_dir):
            continue
        per_bench = {}
        rt_per_bench = {}
        for name in scenario.BENCH_NAMES:
            bench_dir = os.path.join(label_dir, name)
            if not os.path.isdir(bench_dir):
                continue
            results = []
            rt_samples = []
            logs = sorted(
                (f for f in os.listdir(bench_dir) if f.startswith("run") and f.endswith(".log")),
                key=lambda f: int(re.search(r"run(\d+)", f).group(1)))
            for f in logs:
                text = open(os.path.join(bench_dir, f)).read()
                samples = bench_base.PERF_RE.findall(text)
                if not samples:
                    results.append(bench_base.RunResult(failed=True, reason="no PERF line in log"))
                    continue
                fps_values = [float(m[1]) for m in samples]
                count = int(samples[-1][2])
                results.append(bench_base.RunResult(fps=sum(fps_values) / len(fps_values),
                                                    draw_count=count))
                if hasattr(scenario, "analyze_rt"):
                    m = scenario.analyze_rt(os.path.join(bench_dir, f))
                    if m is not None:
                        rt_samples.append(m)
            per_bench[name] = results
            if rt_samples and hasattr(scenario, "aggregate_rt"):
                rt_per_bench[name] = scenario.aggregate_rt(rt_samples)
        if per_bench:
            results_by_label[label] = per_bench
            rt_by_label[label] = rt_per_bench

    if not results_by_label:
        print("no archived logs found under", runs_root, file=sys.stderr)
        sys.exit(1)
    for label, benches in results_by_label.items():
        for n, v in benches.items():
            print(label, n, [repr(r) for r in v])
    scenario.write_report(results_by_label, rt_by_label, scenario.BENCH_NAMES)


if __name__ == "__main__":
    main()

