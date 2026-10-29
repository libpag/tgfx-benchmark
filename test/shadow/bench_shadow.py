#!/usr/bin/env python3
"""
Shadow-style benchmark scenario for LayerBench.

Measures the performance of the tgfx Layer module's DropShadowStyle and
InnerShadowStyle on solid-color rounded-rect layers, for each (label, tgfx_dir)
configuration listed in CONFIGS, and writes a report to REPORT_PATH.

Every run produces two metric families from the same stdout log:
  - fps / drawCount from PERF lines (printed once the bench stabilizes),
  - RenderTarget metrics from per-frame PROBE lines (rtCreated cumulative,
    rtAlive live count): rt_alive at stabilization, rt_alive / drawCount,
    and stable-phase creations per frame.

Usage:
  python3 test/shadow/bench_shadow.py
"""

import os
import re
import shutil
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                 ".codebuddy", "skills", "bench-report", "scripts"))
import bench_base

# (label, tgfx_dir relative to the project root). Entries whose tgfx_dir does
# not exist are skipped, so the baseline can be measured before the optimized
# worktree is available.
CONFIGS = [
    ("baseline", "../tgfx"),
    ("candidate", "../TGFX_WT_2"),
]

BENCH_NAMES = [
    "LayerBench-DropShadow",
    "LayerBench-InnerShadow",
]

RUNS = 5

REPORT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "PERF_LAYER_SHADOW.md")

PROBE_RE = re.compile(r"PROBE frame=(\d+) rtCreated=(\d+) rtAlive=(\d+)")


def analyze_rt(log_file):
    """Extracts RenderTarget metrics from one run log.

    Returns {rt_alive, alive_per_layer, creations_per_frame} or None.
    """
    frames = []
    perf_line_index = None
    draw_count = 0
    with open(log_file) as fp:
        for line in fp:
            m = PROBE_RE.search(line)
            if m:
                frames.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
                continue
            pm = bench_base.PERF_RE.search(line)
            if pm:
                if perf_line_index is None:
                    perf_line_index = len(frames) - 1
                draw_count = int(pm.group(3))
    if len(frames) < 2 or draw_count == 0:
        return None
    split = perf_line_index if perf_line_index is not None else len(frames) - 1
    stable = [frames[i][1] - frames[i - 1][1] for i in range(split + 1, len(frames))]
    if not stable:
        return None
    rt_alive = frames[split][2]
    return {
        "rt_alive": rt_alive,
        "alive_per_layer": rt_alive / draw_count,
        "creations_per_frame": sum(stable) / len(stable),
    }


def aggregate_rt(samples):
    keys = ["rt_alive", "alive_per_layer", "creations_per_frame"]
    out = {"rounds": len(samples)}
    for k in keys:
        values = [s[k] for s in samples]
        out[k] = {"avg": sum(values) / len(values), "min": min(values), "max": max(values)}
    return out


def fmt_change(base, value):
    diff = value - base
    if base == 0:
        return "n/a"
    sign = "+" if diff >= 0 else ""
    return f"{sign}{diff:.2f} ({sign}{diff / base * 100:.2f}%)"


def write_report(results_by_label, rt_by_label, bench_names):
    labels = list(results_by_label.keys())
    lines = []
    lines.append("# LayerBench 阴影样式性能测试报告")
    lines.append("")
    lines.append("## 1 背景")
    lines.append("")
    if len(labels) == 1:
        lines.append("测量 tgfx Layer 模块阴影样式（DropShadowStyle / InnerShadowStyle）的性能基线，"
                     "为后续优化对比建立基准。")
    else:
        lines.append("对比 tgfx Layer 模块阴影样式（DropShadowStyle / InnerShadowStyle）优化前后的性能差异。")
    lines.append("")
    lines.append("- 测试对象：`python3 test/shadow/bench_shadow.py`（用例：LayerBench-DropShadow、LayerBench-InnerShadow）")
    lines.append(f"- 统计方式：每个用例每配置连续运行 {RUNS} 轮；进程达到稳定（isMaxDrawCountReached）后"
                 "进入 5 秒采样窗口，fps 取窗口内采样均值，count 取窗口内最后一次 drawCount；"
                 "RenderTarget 指标与 fps/count 来自同一批运行（每帧 PROBE 行）")
    lines.append("- 用例参数：SolidLayer 粒子，尺寸 4~14 x density 随机，圆角按索引交替 0 / 宽 x 0.25；"
                 "阴影 blur = 短边 x 1/4，offset = 短边 x 1/4，spread = 短边 x 1/8，颜色 (1,0,0,0.5)")
    lines.append("- 原始日志：runs/<label>/<用例名>/run<N>.log（报告生成后清理，报告为唯一留档）")
    lines.append("")
    lines.append("## 2 结果汇总（平均值）")
    lines.append("")
    if len(labels) == 2:
        base_label, cand_label = labels
        lines.append("| 用例 | baseline fps | candidate fps | fps 变化 | baseline count | candidate count | count 变化 |")
        lines.append("|---|---:|---:|---:|---:|---:|---:|")
        for name in bench_names:
            b = bench_base.summarise(results_by_label[base_label].get(name, []))
            c = bench_base.summarise(results_by_label[cand_label].get(name, []))
            if b and c:
                lines.append(f"| {name} | {b[0]:.2f} | {c[0]:.2f} | {fmt_change(b[0], c[0])} "
                             f"| {b[1]:.0f} | {c[1]:.0f} | {fmt_change(b[1], c[1])} |")
            else:
                lines.append(f"| {name} | - | - | 数据不完整 | - | - | - |")
        rt_base = rt_by_label.get(base_label, {})
        rt_cand = rt_by_label.get(cand_label, {})
        if rt_base and rt_cand:
            lines.append("")
            lines.append("RenderTarget 指标（与 fps/count 同一批运行测得，5 轮均值；"
                         "rtAlive = 稳定时存活的 RenderTarget 数（含缓存中未使用的实例），"
                         "每帧新建 = 稳定期每帧新建的 RenderTarget 数）：")
            lines.append("")
            lines.append("| 用例 | baseline<br>rtAlive | candidate<br>rtAlive | rtAlive<br>变化 | baseline<br>rtAlive/图层 | candidate<br>rtAlive/图层 | rtAlive/图层<br>变化 | baseline<br>每帧新建 | candidate<br>每帧新建 | 每帧新建<br>变化 |")
            lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
            for name in bench_names:
                rb = rt_base.get(name)
                rc = rt_cand.get(name)
                if rb and rc:
                    alive_b = rb["rt_alive"]["avg"]
                    alive_c = rc["rt_alive"]["avg"]
                    ratio_b = rb["alive_per_layer"]["avg"]
                    ratio_c = rc["alive_per_layer"]["avg"]
                    churn_b = rb["creations_per_frame"]["avg"]
                    churn_c = rc["creations_per_frame"]["avg"]
                    lines.append(f"| {name} | {alive_b:.0f} | {alive_c:.0f} | {fmt_change(alive_b, alive_c)}"
                                 f" | {ratio_b:.3f} | {ratio_c:.3f} | {fmt_change(ratio_b, ratio_c)}"
                                 f" | {churn_b:.2f} | {churn_c:.2f} | {fmt_change(churn_b, churn_c)} |")
                else:
                    lines.append(f"| {name} | - | - | - | - | - | - | - | - | - |")
    else:
        lines.append("| 用例 | fps | count |")
        lines.append("|---|---:|---:|")
        for name in bench_names:
            s = bench_base.summarise(results_by_label[labels[0]].get(name, []))
            if s:
                lines.append(f"| {name} | {s[0]:.2f} | {s[1]:.0f} |")
            else:
                lines.append(f"| {name} | - | - |")
        rt_single = rt_by_label.get(labels[0], {})
        if rt_single:
            lines.append("")
            lines.append("RenderTarget 指标（与 fps/count 同一批运行测得，5 轮均值；"
                         "rtAlive = 稳定时存活的 RenderTarget 数（含缓存中未使用的实例），"
                         "每帧新建 = 稳定期每帧新建的 RenderTarget 数）：")
            lines.append("")
            lines.append("| 用例 | rtAlive | rtAlive/图层 | 每帧新建 |")
            lines.append("|---|---:|---:|---:|")
            for name in bench_names:
                m = rt_single.get(name)
                if m:
                    lines.append(f"| {name} | {m['rt_alive']['avg']:.0f} "
                                 f"| {m['alive_per_layer']['avg']:.3f} "
                                 f"| {m['creations_per_frame']['avg']:.2f} |")
                else:
                    lines.append(f"| {name} | - | - | - |")
    lines.append("")
    lines.append("## 3 详细数据")
    config_dirs = dict((c[0], c[1]) for c in CONFIGS)
    for li, label in enumerate(labels, 1):
        results = results_by_label[label]
        lines.append("")
        lines.append(f"### 3.{li} {label}（TGFX_DIR={config_dirs[label]}）")
        for name in bench_names:
            runs = results.get(name)
            if not runs:
                continue
            lines.append("")
            lines.append(f"#### {name}")
            lines.append("")
            for i, r in enumerate(runs, 1):
                if r.failed:
                    lines.append(f"- Run{i}: 失败（{r.reason}）")
                else:
                    lines.append(f"- Run{i}: fps={r.fps:.2f}, count={r.draw_count}")
            valid = [r for r in runs if not r.failed]
            if valid:
                fps_values = [r.fps for r in valid]
                fps_avg = sum(fps_values) / len(fps_values)
                count_avg = sum(r.draw_count for r in valid) / len(valid)
                suffix = f"（基于 {len(valid)} 轮有效）" if len(valid) != len(runs) else ""
                lines.append(f"- 平均：fps={fps_avg:.2f}（min={min(fps_values):.2f}, max={max(fps_values):.2f}）"
                             f", count={count_avg:.0f}{suffix}")
            else:
                lines.append("- 无有效数据")
    lines.append("")
    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    with open(REPORT_PATH, "w") as fp:
        fp.write("\n".join(lines))
    print(f"\n报告已写入: {REPORT_PATH}", flush=True)


def main():
    results_by_label = {}
    rt_by_label = {}
    for label, tgfx_dir in CONFIGS:
        abs_dir = os.path.normpath(os.path.join(bench_base.PROJECT_ROOT, tgfx_dir))
        if not os.path.isdir(abs_dir):
            print(f"[{label}] tgfx 路径不存在，跳过: {abs_dir}", flush=True)
            continue
        binary = bench_base.configure_and_build(label, tgfx_dir)
        if binary is None:
            print(f"[{label}] 构建失败，跳过", flush=True)
            continue
        results_by_label[label] = {}
        rt_by_label[label] = {}
        for name in BENCH_NAMES:
            results, aborted = bench_base.run_bench_name(binary, label, name, RUNS)
            results_by_label[label][name] = results
            # RenderTarget metrics from the same run logs.
            samples = []
            log_dir = os.path.join(bench_base.PROJECT_ROOT, "runs", label, name)
            for i in range(RUNS):
                log_file = os.path.join(log_dir, f"run{i + 1}.log")
                if not os.path.isfile(log_file):
                    continue
                m = analyze_rt(log_file)
                if m is None:
                    print(f"  [{label}] {name} run{i + 1}: 无 PROBE 数据", flush=True)
                    continue
                samples.append(m)
            if samples:
                rt_by_label[label][name] = aggregate_rt(samples)
                agg = rt_by_label[label][name]
                print(f"  [{label}] {name} RT: rtAlive={agg['rt_alive']['avg']:.0f} "
                      f"alive/layer={agg['alive_per_layer']['avg']:.3f} "
                      f"create/frame={agg['creations_per_frame']['avg']:.2f}", flush=True)
            if aborted:
                break
    if not results_by_label:
        print("没有任何配置产生数据", flush=True)
        sys.exit(1)
    write_report(results_by_label, rt_by_label, BENCH_NAMES)
    # 报告写完后清理原始日志，报告为唯一留档
    runs_root = os.path.join(bench_base.PROJECT_ROOT, "runs")
    for label in results_by_label:
        shutil.rmtree(os.path.join(runs_root, label), ignore_errors=True)
    print("原始日志已清理: " + runs_root, flush=True)


if __name__ == "__main__":
    main()

