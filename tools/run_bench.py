#!/usr/bin/env python3
"""
Benchmark driver for tgfx-benchmark.

Launches the Benchmark binary with a chosen BENCH_NAME, watches stdout for
"PERF name=... fps=... drawCount=... stable=..." lines, and applies the
following protocol per run:

  1. Wait up to STABLE_TIMEOUT_SEC for stable=1.
       - If timed out, mark this run as failed and continue.
  2. After stable=1, sample for SAMPLE_AFTER_STABLE_SEC, collecting every
     PERF line.
  3. Record fps as the mean of all fps samples in that window, and
     drawCount as the last drawCount seen in that window.
  4. SIGTERM the process and wait COOLDOWN_SEC before the next run.

If FAIL_LIMIT consecutive runs fail, abort the whole task.
"""

import os
import re
import signal
import subprocess
import sys
import threading
import time

BENCH_BIN = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..",
    "cmake-build-releaselocal",
    "Benchmark",
)
BENCH_BIN = os.path.normpath(BENCH_BIN)

STABLE_TIMEOUT_SEC = 30.0
SAMPLE_AFTER_STABLE_SEC = 5.0
COOLDOWN_SEC = 5.0
RUNS_PER_TYPE = 10
FAIL_LIMIT = 3

PERF_RE = re.compile(
    r"PERF name=(\S+) fps=([\d.]+) drawCount=(\d+) stable=(\d)"
)

ALL_TYPES = [
    "ParticleBench-Rect",
    "ParticleBench-Circle",
    "ParticleBench-Oval",
    "ParticleBench-RRect",
]


class RunResult:
    def __init__(self, fps=0.0, draw_count=0, failed=False, reason=""):
        self.fps = fps
        self.draw_count = draw_count
        self.failed = failed
        self.reason = reason

    def __repr__(self):
        if self.failed:
            return f"FAILED ({self.reason})"
        return f"fps={self.fps:6.2f}  count={self.draw_count}"


def run_one(bench_name):
    env = os.environ.copy()
    env["BENCH_NAME"] = bench_name
    proc = subprocess.Popen(
        [BENCH_BIN],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
        text=True,
        bufsize=1,
    )

    state = {
        "fps": 0.0,
        "drawCount": 0,
        "stable": 0,
        "stableSince": None,
        "samples_fps": [],
        "samples_last_count": 0,
    }
    state_lock = threading.Lock()
    stop_reading = threading.Event()

    def reader():
        try:
            for line in proc.stdout:
                m = PERF_RE.search(line)
                if not m:
                    continue
                fps = float(m.group(2))
                draw_count = int(m.group(3))
                stable = int(m.group(4))
                with state_lock:
                    state["fps"] = fps
                    state["drawCount"] = draw_count
                    state["stable"] = stable
                    if stable == 1 and state["stableSince"] is None:
                        state["stableSince"] = time.time()
                    if state["stableSince"] is not None:
                        state["samples_fps"].append(fps)
                        state["samples_last_count"] = draw_count
                if stop_reading.is_set():
                    break
        except Exception:
            pass

    t = threading.Thread(target=reader, daemon=True)
    t.start()

    started = time.time()
    result = None
    try:
        while True:
            time.sleep(0.2)
            if proc.poll() is not None:
                result = RunResult(failed=True, reason="process exited unexpectedly")
                break
            with state_lock:
                stable_since = state["stableSince"]
            if stable_since is None:
                if time.time() - started > STABLE_TIMEOUT_SEC:
                    result = RunResult(
                        failed=True,
                        reason=f"not stable within {STABLE_TIMEOUT_SEC:.0f}s",
                    )
                    break
            else:
                if time.time() - stable_since >= SAMPLE_AFTER_STABLE_SEC:
                    with state_lock:
                        samples = list(state["samples_fps"])
                        last_count = state["samples_last_count"]
                    fps_mean = sum(samples) / len(samples) if samples else 0.0
                    result = RunResult(fps=fps_mean, draw_count=last_count)
                    break
    finally:
        stop_reading.set()
        try:
            proc.send_signal(signal.SIGTERM)
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            try:
                proc.wait(timeout=3)
            except Exception:
                pass
        except Exception:
            pass

    return result


def run_type(bench_name, runs):
    print(f"\n===== {bench_name} =====", flush=True)
    results = []
    consecutive_fail = 0
    for i in range(runs):
        print(f"  Run {i + 1}/{runs} ...", end=" ", flush=True)
        r = run_one(bench_name)
        print(repr(r), flush=True)
        results.append(r)
        if r.failed:
            consecutive_fail += 1
            if consecutive_fail >= FAIL_LIMIT:
                print(
                    f"  连续 {FAIL_LIMIT} 轮失败,终止整个任务",
                    flush=True,
                )
                return results, True
        else:
            consecutive_fail = 0
        time.sleep(COOLDOWN_SEC)
    return results, False


def summarise(name, results):
    valid = [r for r in results if not r.failed]
    print(f"\n----- {name} 汇总 -----", flush=True)
    for i, r in enumerate(results, 1):
        print(f"  Run {i:2d}: {r!r}", flush=True)
    if not valid:
        print("  无有效数据", flush=True)
        return
    fps_avg = sum(r.fps for r in valid) / len(valid)
    count_avg = sum(r.draw_count for r in valid) / len(valid)
    print(
        f"  平均 (基于 {len(valid)} 次有效): fps={fps_avg:6.2f}  count={count_avg:.0f}",
        flush=True,
    )


def main():
    if len(sys.argv) < 2:
        print("usage: run_bench.py <BenchName|All>", file=sys.stderr)
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
