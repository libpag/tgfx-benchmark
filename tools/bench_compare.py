#!/usr/bin/env python3
"""Compare two or more benchmark result files produced by the Benchmark binary.

The tool keeps three concerns separate:
  1. Comparability - the two runs must describe the same scene, tier and parameters, and must
     have rendered the same image. Otherwise no timing conclusion is drawn.
  2. Effect size - the ratio of the medians of the primary metric.
  3. Significance - a permutation test on the per repeat values, corrected for the number of
     comparisons, so that running many scenes does not manufacture false positives.
"""

import argparse
import itertools
import json
import math
import os
import random
import statistics
import sys

DEFAULT_METRIC = "throughput_ms"
CATEGORIES = ("prim", "pag", "ui", "vec", "doc", "res")


def load_result(path):
    with open(path, "r", encoding="utf-8") as stream:
        return json.load(stream)


def case_key(result):
    return (result["scene"], result["tier"], result.get("param_hash", ""))


def index_results(document):
    indexed = {}
    for result in document.get("results", []):
        indexed[case_key(result)] = result
    return indexed


def effect_values(result, metric):
    """Values used for the effect size. One independent sample per repeat keeps the effect size
    and the hypothesis test on the same data instead of mixing two noisy estimators."""
    if metric == "throughput_ms":
        values = result.get("throughput_ms", {}).get("per_repeat") or []
        if not values:
            median = result.get("throughput_ms", {}).get("median", 0.0)
            values = [median] if median else []
        return [float(value) for value in values]
    if metric == "frame_ms":
        value = result.get("frame_ms", {}).get("median", 0.0)
        return [float(value)]
    if metric == "gpu_ms":
        value = result.get("gpu_ms", {}).get("median", 0.0)
        return [float(value)] if value else []
    raise ValueError("unsupported metric: " + metric)


def significance_values(result, metric):
    """Independent samples used for the hypothesis test. One value per repeat, because blocks
    inside a repeat are autocorrelated and would make the test far too optimistic."""
    if metric == "throughput_ms":
        # Blocks and raw frames are autocorrelated within a run, so they are used for the effect
        # size only. A hypothesis test on them would report certainty about noise.
        values = result.get("throughput_ms", {}).get("per_repeat") or []
        if not values:
            return effect_values(result, metric)
        return [float(value) for value in values]
    return effect_values(result, metric)
    if metric == "frame_ms":
        value = result.get("frame_ms", {}).get("median", 0.0)
        return [float(value)]
    if metric == "gpu_ms":
        value = result.get("gpu_ms", {}).get("median", 0.0)
        return [float(value)] if value else []
    raise ValueError("unsupported metric: " + metric)


def median(values):
    return statistics.median(values) if values else 0.0


def permutation_p_value(base, head, samples=20000, seed=7):
    """Two sided permutation test on the difference of medians."""
    pooled = list(base) + list(head)
    n = len(base)
    if n == 0 or len(head) == 0:
        return 1.0
    observed = abs(median(head) - median(base))
    if observed == 0.0:
        return 1.0

    tolerance = 1e-9
    if math.comb(len(pooled), n) <= samples * 4:
        count = 0
        total = 0
        for indices in itertools.combinations(range(len(pooled)), n):
            chosen = set(indices)
            left = [pooled[i] for i in range(len(pooled)) if i in chosen]
            right = [pooled[i] for i in range(len(pooled)) if i not in chosen]
            total += 1
            if abs(median(right) - median(left)) >= observed - tolerance:
                count += 1
        return count / total if total else 1.0

    generator = random.Random(seed)
    count = 0
    for _ in range(samples):
        generator.shuffle(pooled)
        left = pooled[:n]
        right = pooled[n:]
        if abs(median(right) - median(left)) >= observed - tolerance:
            count += 1
    return (count + 1) / (samples + 1)


def paired_permutation_p_value(base, head, samples=20000, seed=3):
    """Sign flip test on the mean of paired differences.

    Runs that were interleaved round by round share the same machine state within a round, so the
    difference between the two sides is the quantity of interest and its sign is exchangeable
    under the null hypothesis.
    """
    count = min(len(base), len(head))
    differences = [head[i] - base[i] for i in range(count)]
    if not differences:
        return 1.0
    observed = abs(sum(differences) / count)
    if observed == 0.0:
        return 1.0
    tolerance = 1e-12
    if count <= 16:
        hits = 0
        total = 0
        for mask in range(1 << count):
            total_value = 0.0
            for index, difference in enumerate(differences):
                total_value += difference if (mask >> index) & 1 else -difference
            total += 1
            if abs(total_value / count) >= observed - tolerance:
                hits += 1
        return hits / total

    generator = random.Random(seed)
    hits = 0
    for _ in range(samples):
        total_value = 0.0
        for difference in differences:
            total_value += difference if generator.random() < 0.5 else -difference
        if abs(total_value / count) >= observed - tolerance:
            hits += 1
    return (hits + 1) / (samples + 1)


def paired_ratio_ci(base, head, samples=4000, seed=5, alpha=0.05):
    count = min(len(base), len(head))
    generator = random.Random(seed)
    ratios = []
    for _ in range(samples):
        indices = [generator.randrange(count) for _ in range(count)]
        left = [base[index] for index in indices]
        right = [head[index] for index in indices]
        left_median = median(left)
        if left_median > 0.0:
            ratios.append(median(right) / left_median)
    if not ratios:
        return (0.0, 0.0)
    ratios.sort()
    low = ratios[int((alpha / 2.0) * len(ratios))]
    high = ratios[min(len(ratios) - 1, int((1.0 - alpha / 2.0) * len(ratios)))]
    return (low, high)


def bootstrap_ratio_ci(base, head, samples=2000, seed=11, alpha=0.05):
    generator = random.Random(seed)
    ratios = []
    for _ in range(samples):
        left = [generator.choice(base) for _ in base]
        right = [generator.choice(head) for _ in head]
        left_median = median(left)
        if left_median <= 0.0:
            continue
        ratios.append(median(right) / left_median)
    if not ratios:
        return (0.0, 0.0)
    ratios.sort()
    low = ratios[int((alpha / 2.0) * len(ratios))]
    high = ratios[min(len(ratios) - 1, int((1.0 - alpha / 2.0) * len(ratios)))]
    return (low, high)


def benjamini_hochberg(p_values):
    count = len(p_values)
    order = sorted(range(count), key=lambda i: p_values[i])
    adjusted = [0.0] * count
    previous = 1.0
    for rank in range(count - 1, -1, -1):
        index = order[rank]
        value = p_values[index] * count / (rank + 1)
        previous = min(previous, value)
        adjusted[index] = min(1.0, previous)
    return adjusted


def read_thumbnail(directory, name, width=64, height=36):
    if not directory or not name:
        return None
    path = os.path.join(directory, name)
    if not os.path.exists(path):
        return None
    with open(path, "rb") as stream:
        data = stream.read()
    if len(data) != width * height * 4:
        return None
    return data


def psnr(left, right):
    if left is None or right is None or len(left) != len(right):
        return None
    squared = 0
    count = 0
    for a, b in zip(left, right):
        delta = int(a) - int(b)
        squared += delta * delta
        count += 1
    if count == 0:
        return None
    mse = squared / count
    if mse == 0.0:
        return 99.0
    return 10.0 * math.log10((255.0 * 255.0) / mse)


def compare_checkpoints(base, head, base_dir, head_dir):
    base_points = {point["frame"]: point for point in base.get("checkpoints", [])}
    head_points = {point["frame"]: point for point in head.get("checkpoints", [])}
    frames = sorted(set(base_points) & set(head_points))
    if not frames:
        return ("missing", None, "no common checkpoint frames")

    worst = None
    for frame in frames:
        left = base_points[frame]
        right = head_points[frame]
        if left["hash"] == right["hash"]:
            continue
        left_data = read_thumbnail(base_dir, left.get("file"))
        right_data = read_thumbnail(head_dir, right.get("file"))
        value = psnr(left_data, right_data)
        if value is None:
            return ("differs", None, "checkpoint %d differs, no thumbnails available" % frame)
        if worst is None or value < worst:
            worst = value
    if worst is None:
        return ("identical", None, "")
    return ("differs", worst, "worst psnr %.1f dB" % worst)


def format_row(columns, widths):
    cells = []
    for index, value in enumerate(columns):
        cells.append(str(value).ljust(widths[index]))
    return "  ".join(cells).rstrip()


def geometric_mean(values):
    filtered = [value for value in values if value > 0.0]
    if not filtered:
        return 1.0
    return math.exp(sum(math.log(value) for value in filtered) / len(filtered))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", help="baseline result json")
    parser.add_argument("head", help="candidate result json")
    parser.add_argument("--metric", default=DEFAULT_METRIC,
                        choices=("throughput_ms", "frame_ms", "gpu_ms"))
    parser.add_argument("--threshold", type=float, default=0.03,
                        help="minimum relative change to report, default 3%%")
    parser.add_argument("--alpha", type=float, default=0.05)
    parser.add_argument("--base-checkpoints", default=None)
    parser.add_argument("--head-checkpoints", default=None)
    parser.add_argument("--require-identical", action="store_true",
                        help="treat any rendering difference as incomparable")
    parser.add_argument("--markdown", default=None)
    parser.add_argument("--html", default=None)
    arguments = parser.parse_args()

    base_document = load_result(arguments.base)
    head_document = load_result(arguments.head)
    base_rounds = base_document.get("config", {}).get("interleaved_rounds")
    head_rounds = head_document.get("config", {}).get("interleaved_rounds")
    paired_rounds = base_rounds if base_rounds and base_rounds == head_rounds else None
    base_index = index_results(base_document)
    head_index = index_results(head_document)

    keys = sorted(set(base_index) & set(head_index))
    rows = []
    p_values = []

    for key in keys:
        base = base_index[key]
        head = head_index[key]
        scene, tier, _ = key
        category = base.get("category") or "prim"

        row = {
            "scene": scene, "tier": tier, "category": category,
            "verdict": "neutral", "detail": "",
            "ratio": None, "low": None, "high": None,
            "base_ms": None, "head_ms": None, "p": None,
        }

        status_problem = None
        if base.get("status") != "ok" or head.get("status") != "ok":
            status_problem = "status %s/%s" % (base.get("status"), head.get("status"))
        if status_problem is not None:
            row["verdict"] = "incomparable"
            row["detail"] = status_problem
            rows.append(row)
            continue

        state, value, detail = compare_checkpoints(
            base, head, arguments.base_checkpoints, arguments.head_checkpoints)
        if state == "differs":
            if value is not None and value >= 40.0:
                row["detail"] = "rendering equivalent (%s)" % detail
            else:
                row["verdict"] = "incomparable"
                row["detail"] = "rendering differs (%s)" % (detail or "hash mismatch")
                rows.append(row)
                continue
        elif state == "missing":
            row["detail"] = detail

        base_values = effect_values(base, arguments.metric)
        head_values = effect_values(head, arguments.metric)
        base_sig = significance_values(base, arguments.metric)
        head_sig = significance_values(head, arguments.metric)
        if not base_values or not head_values:
            row["verdict"] = "incomparable"
            row["detail"] = "no samples for metric " + arguments.metric
            rows.append(row)
            continue

        base_median = median(base_values)
        head_median = median(head_values)
        row["base_ms"] = base_median
        row["head_ms"] = head_median
        if base_median <= 0.0:
            row["verdict"] = "incomparable"
            row["detail"] = "zero baseline"
            rows.append(row)
            continue

        ratio = head_median / base_median
        paired = (paired_rounds is not None and
                  len(base_sig) == len(head_sig) and len(base_sig) >= 4)
        if paired:
            low, high = paired_ratio_ci(base_sig, head_sig)
            p_value = paired_permutation_p_value(base_sig, head_sig)
        else:
            low, high = bootstrap_ratio_ci(base_sig, head_sig)
            p_value = permutation_p_value(base_sig, head_sig)
        row["ratio"] = ratio
        row["low"] = low
        row["high"] = high
        row["p"] = p_value
        p_values.append((len(rows), p_value))
        rows.append(row)

    adjusted = {}
    if p_values:
        values = [value for _, value in p_values]
        corrected = benjamini_hochberg(values)
        for (row_index, _), value in zip(p_values, corrected):
            adjusted[row_index] = value

    for index, row in enumerate(rows):
        if row["verdict"] == "incomparable":
            continue
        p_value = adjusted.get(index, 1.0)
        row["p_adjusted"] = p_value
        ratio = row["ratio"]
        if p_value < arguments.alpha:
            if ratio > 1.0 + arguments.threshold:
                row["verdict"] = "regression"
            elif ratio < 1.0 - arguments.threshold:
                row["verdict"] = "improvement"
            else:
                row["verdict"] = "neutral"
        else:
            row["verdict"] = "neutral"

    widths = [10, 4, 9, 9, 7, 17, 12]
    header = ["scene", "tier", "base_ms", "head_ms", "ratio", "95% ci", "verdict"]
    print(format_row(header, widths))
    print("-" * sum(widths))
    for row in rows:
        if row["base_ms"] is None:
            cells = [row["scene"], row["tier"], "-", "-", "-", "-", row["verdict"].upper()]
        else:
            cells = [
                row["scene"], row["tier"],
                "%.3f" % row["base_ms"], "%.3f" % row["head_ms"],
                "%.3f" % row["ratio"],
                "[%.3f, %.3f]" % (row["low"], row["high"]),
                row["verdict"].upper(),
            ]
        print(format_row(cells, widths))
        if row["detail"]:
            print(" " * 12 + row["detail"])

    summary = {}
    for row in rows:
        if row["verdict"] == "incomparable" or row["ratio"] is None:
            continue
        summary.setdefault(row["category"], []).append(row["ratio"])
    print()
    parts = []
    for category in CATEGORIES:
        if category in summary:
            parts.append("%s %.3f" % (category, geometric_mean(summary[category])))
    primary = {name: values for name, values in summary.items() if name != "prim"}
    if not primary:
        primary = summary
    overall = geometric_mean([ratio for values in primary.values() for ratio in values])
    print("geomean  " + " | ".join(parts) + " | overall %.3f" % overall)

    regressions = [row for row in rows if row["verdict"] == "regression"]
    incomparable = [row for row in rows if row["verdict"] == "incomparable"]

    if arguments.markdown:
        with open(arguments.markdown, "w", encoding="utf-8") as stream:
            stream.write("# Benchmark comparison\n\n")
            stream.write("| scene | tier | base ms | head ms | ratio | 95% CI | verdict |\n")
            stream.write("| --- | --- | --- | --- | --- | --- | --- |\n")
            for row in rows:
                if row["base_ms"] is None:
                    stream.write("| %s | %s | - | - | - | - | %s |\n" %
                                 (row["scene"], row["tier"], row["verdict"]))
                else:
                    stream.write("| %s | %s | %.3f | %.3f | %.3f | [%.3f, %.3f] | %s |\n" % (
                        row["scene"], row["tier"], row["base_ms"], row["head_ms"],
                        row["ratio"], row["low"], row["high"], row["verdict"]))

    if arguments.html:
        write_html(arguments.html, rows, overall, summary, base_document, head_document)

    if incomparable and arguments.require_identical:
        return 2
    return 1 if regressions else 0


def write_html(path, rows, overall, summary, base_document, head_document):
    colors = {"regression": "#c0392b", "improvement": "#1e8449",
              "neutral": "#2c3e50", "incomparable": "#7f8c8d"}
    parts = []
    parts.append("<!DOCTYPE html><html><head><meta charset='utf-8'>")
    parts.append("<title>Benchmark comparison</title><style>")
    parts.append("body{font-family:-apple-system,Segoe UI,sans-serif;margin:32px;color:#222}")
    parts.append("table{border-collapse:collapse;width:100%;font-size:13px}")
    parts.append("th,td{border-bottom:1px solid #e3e3e3;padding:6px 8px;text-align:left}")
    parts.append("th{background:#f6f7f9;font-weight:600}")
    parts.append(".bar{height:8px;background:#3498db;display:inline-block}")
    parts.append("</style></head><body>")
    parts.append("<h1>Benchmark comparison</h1>")
    parts.append("<p>baseline: %s (tgfx %s) &nbsp; candidate: %s (tgfx %s)</p>" % (
        base_document.get("run_id", "?"),
        base_document.get("env", {}).get("tgfx_commit", "?")[:8],
        head_document.get("run_id", "?"),
        head_document.get("env", {}).get("tgfx_commit", "?")[:8]))
    parts.append("<p>overall geometric mean ratio: <b>%.3f</b></p>" % overall)
    parts.append("<table><tr><th>scene</th><th>tier</th><th>base ms</th><th>head ms</th>"
                 "<th>ratio</th><th>95% CI</th><th>verdict</th></tr>")
    for row in rows:
        color = colors.get(row["verdict"], "#2c3e50")
        if row["base_ms"] is None:
            cells = (row["scene"], row["tier"], "-", "-", "-", "-")
        else:
            cells = (row["scene"], row["tier"], "%.3f" % row["base_ms"],
                     "%.3f" % row["head_ms"], "%.3f" % row["ratio"],
                     "[%.3f, %.3f]" % (row["low"], row["high"]))
        parts.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td>"
                     "<td style='color:%s;font-weight:600'>%s</td></tr>" % (
                         cells[0], cells[1], cells[2], cells[3], cells[4], cells[5],
                         color, row["verdict"]))
    parts.append("</table></body></html>")
    with open(path, "w", encoding="utf-8") as stream:
        stream.write("".join(parts))


if __name__ == "__main__":
    sys.exit(main())
