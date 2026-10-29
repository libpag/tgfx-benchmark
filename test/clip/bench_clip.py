#!/usr/bin/env python3
"""
Ad-hoc benchmark driver for tgfx-benchmark.

Runs benches directly against the prebuilt cmake-build-releaselocal/Benchmark
binary (no configure/build step, no per-run log files). The stability protocol
is shared with scenario scripts via bench_base:

  1. Wait up to STABLE_TIMEOUT_SEC for the first PERF line (printed only after
     the bench reports isMaxDrawCountReached()).
       - If timed out, mark this run as failed and continue.
  2. After the first PERF line, sample for SAMPLE_AFTER_STABLE_SEC, collecting
     every PERF line.
  3. Record fps as the mean of all fps samples in that window, and drawCount
     as the last drawCount seen in that window.
  4. SIGTERM the process and wait COOLDOWN_SEC before the next run.

If FAIL_LIMIT consecutive runs fail, abort the whole task.
"""

import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                 ".codebuddy", "skills", "bench-report", "scripts"))
import bench_base

BENCH_BIN = os.path.join(bench_base.PROJECT_ROOT, "cmake-build-releaselocal", "Benchmark")

RUNS_PER_TYPE = 10

ALL_TYPES = [
    "ParticleBench-Rect",
    "ParticleBench-Circle",
    "ParticleBench-Oval",
    "ParticleBench-RRect",
    "LayerBench-DropShadow",
    "LayerBench-InnerShadow",
]


def run_type(bench_name, runs):
    print(f"\n===== {bench_name} =====", flush=True)
    results = []
    consecutive_fail = 0
    for i in range(runs):
        print(f"  Run {i + 1}/{runs} ...", end=" ", flush=True)
        r = bench_base.run_one(BENCH_BIN, bench_name)
        print(repr(r), flush=True)
        results.append(r)
        if r.failed:
            consecutive_fail += 1
            if consecutive_fail >= bench_base.FAIL_LIMIT:
                print(f"  {bench_base.FAIL_LIMIT} consecutive failures, aborting", flush=True)
                return results, True
        else:
            consecutive_fail = 0
        time.sleep(bench_base.COOLDOWN_SEC)
    return results, False


def summarise(name, results):
    print(f"\n----- {name} 汇总 -----", flush=True)
    for i, r in enumerate(results, 1):
        print(f"  Run {i:2d}: {r!r}", flush=True)
    summary = bench_base.summarise(results)
    if summary is None:
        print("  无有效数据", flush=True)
        return
    fps_avg, count_avg, valid_runs = summary
    print(f"  平均 (基于 {valid_runs} 次有效): fps={fps_avg:6.2f}  count={count_avg:.0f}", flush=True)


def main():
    if len(sys.argv) < 2:
        print("usage: bench_clip.py <BenchName|All>", file=sys.stderr)
        print(f"  BenchName options: {', '.join(ALL_TYPES)}", file=sys.stderr)
        sys.exit(2)
    target = sys.argv[1]

    if target == "All":
        targets = ALL_TYPES
    elif target in ALL_TYPES:
        targets = [target]
    else:
        print(f"unknown bench name: {target}", file=sys.stderr)
        sys.exit(2)

    if not os.path.isfile(BENCH_BIN) or not os.access(BENCH_BIN, os.X_OK):
        print(f"Benchmark binary not found or not executable: {BENCH_BIN}", file=sys.stderr)
        sys.exit(2)

    summary = {}
    aborted = False
    for name in targets:
        results, aborted = run_type(name, RUNS_PER_TYPE)
        summary[name] = results
        if aborted:
            break

    print("\n========= 最终结果 =========", flush=True)
    for name, results in summary.items():
        summarise(name, results)
    if aborted:
        print("\n任务因连续失败提前终止", flush=True)


if __name__ == "__main__":
    main()
