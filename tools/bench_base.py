#!/usr/bin/env python3
"""
Shared benchmark driver for tgfx-benchmark scenario scripts.

A scenario script provides:
  - a list of (label, tgfx_dir) build configurations,
  - a list of bench names to run,
  - the number of runs per bench name.

For each configuration this module configures and builds a dedicated build
directory (cmake-build-bench-<label>), archives the binary and per-run logs
under runs/<label>/, then executes the benchmark protocol per run:

  1. Launch the Benchmark binary with BENCH_NAME=<bench name>.
  2. Wait up to STABLE_TIMEOUT_SEC for the first PERF line (printed only after
     the bench reports isMaxDrawCountReached()).
       - If timed out, mark this run as failed and continue.
  3. After the first PERF line, sample for SAMPLE_AFTER_STABLE_SEC, collecting
     every PERF line.
  4. Record fps as the mean of all fps samples in that window, and drawCount
     as the last drawCount seen in that window.
  5. SIGTERM the process and wait COOLDOWN_SEC before the next run.

If FAIL_LIMIT consecutive runs fail for a bench name, the whole task aborts.
"""

import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time

PROJECT_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

STABLE_TIMEOUT_SEC = 30.0
SAMPLE_AFTER_STABLE_SEC = 5.0
COOLDOWN_SEC = 5.0
FAIL_LIMIT = 3

PERF_RE = re.compile(
    r"PERF name=(\S+) fps=([\d.]+) drawCount=(\d+) stable=(\d)"
)


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


def build_dir_for(label):
    return os.path.join(PROJECT_ROOT, f"cmake-build-bench-{label}")


def configure_and_build(label, tgfx_dir):
    """Configures (once) and builds the Benchmark target for the given tgfx path.

    Returns the path to the built binary, or None on failure.
    """
    build_dir = build_dir_for(label)
    binary = os.path.join(build_dir, "Benchmark")
    if not os.path.isfile(os.path.join(build_dir, "CMakeCache.txt")):
        os.makedirs(build_dir, exist_ok=True)
        configure_cmd = [
            "cmake",
            "-S", PROJECT_ROOT,
            "-B", build_dir,
            "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DTGFX_DIR={tgfx_dir}",
        ]
        print(f"[{label}] configure: {' '.join(configure_cmd)}", flush=True)
        if subprocess.run(configure_cmd).returncode != 0:
            print(f"[{label}] cmake configure failed", flush=True)
            return None
    print(f"[{label}] building Benchmark ...", flush=True)
    if subprocess.run(["cmake", "--build", build_dir, "--target", "Benchmark", "-j", "10"]).returncode != 0:
        print(f"[{label}] build failed", flush=True)
        return None
    if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
        print(f"[{label}] binary missing after build: {binary}", flush=True)
        return None
    return binary


def run_one(binary, bench_name, log_file=None):
    env = os.environ.copy()
    env["BENCH_NAME"] = bench_name
    log_fp = open(log_file, "w") if log_file is not None else None
    proc = subprocess.Popen(
        [binary],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
        text=True,
        bufsize=1,
    )

    state = {
        "stableSince": None,
        "samples_fps": [],
        "samples_last_count": 0,
    }
    state_lock = threading.Lock()

    def reader():
        try:
            for line in proc.stdout:
                if log_fp is not None:
                    log_fp.write(line)
                    log_fp.flush()
                m = PERF_RE.search(line)
                if not m:
                    continue
                fps = float(m.group(2))
                draw_count = int(m.group(3))
                with state_lock:
                    if state["stableSince"] is None:
                        state["stableSince"] = time.time()
                    state["samples_fps"].append(fps)
                    state["samples_last_count"] = draw_count
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
        t.join(timeout=3)
        if log_fp is not None:
            log_fp.close()

    return result


def run_bench_name(binary, label, bench_name, runs):
    """Runs one bench name for the given number of rounds.

    Returns (results, aborted). Raw stdout of every run is archived under
    runs/<label>/<bench_name>/run<N>.log.
    """
    print(f"\n===== [{label}] {bench_name} =====", flush=True)
    log_dir = os.path.join(PROJECT_ROOT, "runs", label, bench_name)
    os.makedirs(log_dir, exist_ok=True)
    results = []
    consecutive_fail = 0
    for i in range(runs):
        print(f"  Run {i + 1}/{runs} ...", end=" ", flush=True)
        log_file = os.path.join(log_dir, f"run{i + 1}.log")
        r = run_one(binary, bench_name, log_file)
        print(repr(r), flush=True)
        results.append(r)
        if r.failed:
            consecutive_fail += 1
            if consecutive_fail >= FAIL_LIMIT:
                print(f"  {FAIL_LIMIT} consecutive failures, aborting", flush=True)
                return results, True
        else:
            consecutive_fail = 0
        time.sleep(COOLDOWN_SEC)
    return results, False


def stat_line(values, fmt):
    if not values:
        return "no valid data"
    avg = sum(values) / len(values)
    return f"avg={fmt.format(avg)}  min={fmt.format(min(values))}  max={fmt.format(max(values))}"


def summarise(results):
    """Returns (fps_avg, count_avg, valid_runs) or None if all runs failed."""
    valid = [r for r in results if not r.failed]
    if not valid:
        return None
    fps_avg = sum(r.fps for r in valid) / len(valid)
    count_avg = sum(r.draw_count for r in valid) / len(valid)
    return fps_avg, count_avg, len(valid)


def run_scenario(label, tgfx_dir, bench_names, runs):
    """Builds the given tgfx configuration and runs every bench name.

    Returns {bench_name: [RunResult, ...]} or None if the build failed.
    """
    binary = configure_and_build(label, tgfx_dir)
    if binary is None:
        return None
    archive_dir = os.path.join(PROJECT_ROOT, "runs", label)
    os.makedirs(archive_dir, exist_ok=True)
    shutil.copy2(binary, os.path.join(archive_dir, "Benchmark"))
    summary = {}
    for name in bench_names:
        results, aborted = run_bench_name(binary, label, name, runs)
        summary[name] = results
        if aborted:
            break
    return summary
