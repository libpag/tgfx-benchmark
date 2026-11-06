#!/usr/bin/env python3
"""Run two Benchmark binaries in an interleaved order and compare the merged results.

Why interleaving matters: the very first process run of a case pays for shader compilation and
the driver's on disk pipeline cache, which can be several times slower than the same case in a
later process. A single A-then-B run would attribute that warm up to whichever binary ran second.
Alternating the two binaries across rounds spreads the effect over both sides instead.

Usage:
    bench_run.py --base build-baseline/Benchmark --head build-head/Benchmark \
                 --rounds 3 --filter 'pag.*' --tier M --out runs/2026-11-06
"""

import argparse
import copy
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def build_command(executable, arguments, out_path, checkpoint_dir):
    command = [
        executable,
        "--mode=measure",
        "--out=" + out_path,
        "--checkpoint-dir=" + checkpoint_dir,
    ]
    command.extend(arguments)
    return command


def run_once(executable, arguments, out_path, checkpoint_dir, verbose):
    command = build_command(executable, arguments, out_path, checkpoint_dir)
    if verbose:
        print("  $ " + " ".join(command))
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if verbose and result.stdout:
        sys.stdout.write(result.stdout.decode("utf-8", "replace"))
    if result.returncode not in (0, 1):
        raise SystemExit("benchmark exited with %d: %s" % (result.returncode, executable))


def merge_documents(documents):
    merged = copy.deepcopy(documents[0])
    by_key = {}
    order = []
    for document in documents:
        for case in document.get("results", []):
            key = (case["scene"], case["tier"], case.get("param_hash", ""))
            if key not in by_key:
                clone = copy.deepcopy(case)
                clone["throughput_ms"]["per_repeat"] = []
                clone["throughput_ms"]["blocks"] = []
                clone.setdefault("samples", {})
                clone["samples"]["frame_ms"] = []
                by_key[key] = clone
                order.append(key)
            target = by_key[key]
            target["throughput_ms"]["per_repeat"].extend(
                case.get("throughput_ms", {}).get("per_repeat", []))
            target["throughput_ms"]["blocks"].extend(
                case.get("throughput_ms", {}).get("blocks", []))
            target["samples"]["frame_ms"].extend(
                case.get("samples", {}).get("frame_ms", []))

    merged["results"] = []
    for key in order:
        case = by_key[key]
        values = sorted(case["throughput_ms"].get("blocks", []) or
                        case["throughput_ms"].get("per_repeat", []))
        if values:
            middle = len(values) // 2
            case["throughput_ms"]["median"] = (
                values[middle] if len(values) % 2 else
                0.5 * (values[middle - 1] + values[middle]))
        merged["results"].append(case)
    return merged


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, help="baseline Benchmark executable")
    parser.add_argument("--head", required=True, help="candidate Benchmark executable")
    parser.add_argument("--rounds", type=int, default=3,
                        help="number of interleaved rounds, default 3")
    parser.add_argument("--out", default="runs/interleaved", help="output directory")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--compare-args", default="",
                        help="extra arguments forwarded to bench_compare.py")
    arguments, benchmark_args = parser.parse_known_args()
    benchmark_args = [item for item in benchmark_args if item != "--"]

    os.makedirs(arguments.out, exist_ok=True)

    base_documents = []
    head_documents = []
    started = time.time()

    for round_index in range(arguments.rounds):
        base_first = round_index % 2 == 0
        sequence = [("base", arguments.base), ("head", arguments.head)]
        if not base_first:
            sequence.reverse()
        for name, executable in sequence:
            stamp = "r%d-%s" % (round_index, name)
            out_path = os.path.join(arguments.out, stamp + ".json")
            checkpoint_dir = os.path.join(arguments.out, "ckpt-" + stamp)
            print("[round %d/%d] %s" % (round_index + 1, arguments.rounds, name))
            run_once(executable, benchmark_args, out_path, checkpoint_dir, arguments.verbose)
            documents = base_documents if name == "base" else head_documents
            with open(out_path, "r", encoding="utf-8") as stream:
                documents.append(json.load(stream))

    base_merged = merge_documents(base_documents)
    head_merged = merge_documents(head_documents)
    base_path = os.path.join(arguments.out, "base-merged.json")
    head_path = os.path.join(arguments.out, "head-merged.json")
    with open(base_path, "w", encoding="utf-8") as stream:
        json.dump(base_merged, stream)
    with open(head_path, "w", encoding="utf-8") as stream:
        json.dump(head_merged, stream)

    print("\nelapsed %.1f s, results in %s\n" % (time.time() - started, arguments.out))
    command = [
        sys.executable,
        os.path.join(HERE, "bench_compare.py"),
        base_path,
        head_path,
        "--base-checkpoints", os.path.join(arguments.out, "ckpt-r0-base"),
        "--head-checkpoints", os.path.join(arguments.out, "ckpt-r0-head"),
        "--markdown", os.path.join(arguments.out, "report.md"),
        "--html", os.path.join(arguments.out, "report.html"),
    ]
    if arguments.compare_args:
        command.extend(arguments.compare_args.split())
    return subprocess.call(command)


if __name__ == "__main__":
    sys.exit(main())
