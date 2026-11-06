#!/usr/bin/env python3
"""Run two Benchmark binaries against each other and report the differences.

Two effects make a naive A-then-B run meaningless:

  * The first process of a binary compiles its pipelines and fills the driver's on disk cache.
  * A long run heats the machine up, so the second half of a session is slower than the first.

Both are handled here: each executable gets a discarded warm up process, and the two executables
take turns per case, so the two sides of a comparison are only seconds apart instead of minutes.

Usage:
    bench_run.py --base build-baseline/Benchmark --head build-head/Benchmark \
                 --rounds 4 --filter 'pag.*' --tier M --out runs/2026-11-06
"""

import argparse
import copy
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def parse_known_flags(arguments):
    flags = {}
    for item in arguments:
        if item.startswith("--") and "=" in item:
            name, value = item[2:].split("=", 1)
            flags.setdefault(name, value)
    return flags


def run_once(executable, arguments, out_path, checkpoint_dir, verbose):
    command = [
        executable,
        "--mode=measure",
        "--out=" + out_path,
        "--checkpoint-dir=" + checkpoint_dir,
    ]
    command.extend(arguments)
    if verbose:
        print("  $ " + " ".join(command))
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if verbose and result.stdout:
        sys.stdout.write(result.stdout.decode("utf-8", "replace"))
    if result.returncode not in (0, 1):
        raise SystemExit("benchmark exited with %d: %s" % (result.returncode, executable))


def load_spec(executable):
    output = subprocess.check_output([executable, "--dump-spec"])
    return json.loads(output.decode("utf-8"))


def matches(scene_id, patterns):
    if not patterns:
        return True
    for pattern in patterns:
        if pattern == scene_id:
            return True
        if pattern.endswith("*") and scene_id.startswith(pattern[:-1]):
            return True
    return False


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
                clone["checkpoints"] = []
                by_key[key] = clone
                order.append(key)
            target = by_key[key]
            target["throughput_ms"]["per_repeat"].extend(
                case.get("throughput_ms", {}).get("per_repeat", []))
            target["throughput_ms"]["blocks"].extend(
                case.get("throughput_ms", {}).get("blocks", []))
            target["samples"]["frame_ms"].extend(
                case.get("samples", {}).get("frame_ms", []))
            if not target["checkpoints"]:
                target["checkpoints"] = case.get("checkpoints", [])

    merged.setdefault("config", {})
    merged["config"]["interleaved_rounds"] = len(documents)
    merged["results"] = []
    for key in order:
        case = by_key[key]
        values = sorted(case["throughput_ms"].get("per_repeat", []))
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
    parser.add_argument("--rounds", type=int, default=4,
                        help="number of interleaved rounds per case, default 4")
    parser.add_argument("--out", default="runs/interleaved", help="output directory")
    parser.add_argument("--no-prewarm", action="store_true",
                        help="skip the discarded processes that prime the pipeline cache")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--compare-args", default="",
                        help="extra arguments forwarded to bench_compare.py")
    arguments, benchmark_args = parser.parse_known_args()
    benchmark_args = [item for item in benchmark_args if item != "--"]

    os.makedirs(arguments.out, exist_ok=True)
    flags = parse_known_flags(benchmark_args)
    patterns = [item for item in flags.get("filter", "").split(",") if item]
    tiers = [item for item in flags.get("tier", "M").split(",") if item]

    spec = load_spec(arguments.base)
    scenes = [scene["id"] for scene in spec.get("scenes", []) if matches(scene["id"], patterns)]
    if not scenes:
        raise SystemExit("no scene matched the filter")

    started = time.time()
    base_documents = []
    head_documents = []
    checkpoint_dirs = {}

    if not arguments.no_prewarm:
        for name, executable in (("base", arguments.base), ("head", arguments.head)):
            print("[prewarm] %s" % name)
            run_once(executable,
                     ["--tier=" + tiers[0], "--warmup=2", "--frames=5", "--repeat=1",
                      "--no-breakdown"],
                     os.path.join(arguments.out, "prewarm-%s.json" % name),
                     os.path.join(arguments.out, "ckpt-prewarm-%s" % name),
                     arguments.verbose)

    for scene in scenes:
        for tier in tiers:
            for round_index in range(arguments.rounds):
                order = [("base", arguments.base), ("head", arguments.head)]
                if round_index % 2 == 1:
                    order.reverse()
                for name, executable in order:
                    stamp = "r%d-%s-%s" % (round_index, scene.replace(".", "_"), tier)
                    out_path = os.path.join(arguments.out, stamp + "-%s.json" % name)
                    checkpoint_dir = os.path.join(arguments.out, "ckpt-" + name)
                    checkpoint_dirs[name] = checkpoint_dir
                    run_args = list(benchmark_args)
                    run_args.append("--filter=" + scene)
                    run_once(executable, run_args, out_path, checkpoint_dir, arguments.verbose)
                    documents = base_documents if name == "base" else head_documents
                    with open(out_path, "r", encoding="utf-8") as stream:
                        documents.append(json.load(stream))
            print("  %-18s %-3s %d rounds" % (scene, tier, arguments.rounds))

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
        "--base-checkpoints", checkpoint_dirs.get("base", ""),
        "--head-checkpoints", checkpoint_dirs.get("head", ""),
        "--markdown", os.path.join(arguments.out, "report.md"),
        "--html", os.path.join(arguments.out, "report.html"),
    ]
    if arguments.compare_args:
        command.extend(arguments.compare_args.split())
    return subprocess.call(command)


if __name__ == "__main__":
    sys.exit(main())
