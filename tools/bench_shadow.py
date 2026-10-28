#!/usr/bin/env python3
"""
Shadow-style benchmark scenario for LayerBench.

Measures the performance of the tgfx Layer module's DropShadowStyle and
InnerShadowStyle on solid-color rounded-rect layers. Runs the bench for each
(label, tgfx_dir) configuration listed in CONFIGS and writes a report to
REPORT_PATH.

Usage:
  python3 tools/run_shadow.py
"""

import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
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

REPORT_PATH = os.path.join(bench_base.PROJECT_ROOT, "reports", "PERF_LAYER_SHADOW.md")


def fmt_change(base, value):
    diff = value - base
    if base == 0:
        return "n/a"
    sign = "+" if diff >= 0 else ""
    return f"{sign}{diff:.2f} ({sign}{diff / base * 100:.2f}%)"


def write_report(results_by_label, bench_names):
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
    lines.append("- 测试对象：`python3 tools/bench_shadow.py`（用例：LayerBench-DropShadow、LayerBench-InnerShadow）")
    lines.append(f"- 统计方式：每个用例每配置连续运行 {RUNS} 轮；进程达到稳定（isMaxDrawCountReached）后"
                 "进入 5 秒采样窗口，fps 取窗口内采样均值，count 取窗口内最后一次 drawCount")
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
    else:
        lines.append("| 用例 | fps | count |")
        lines.append("|---|---:|---:|")
        for name in bench_names:
            s = bench_base.summarise(results_by_label[labels[0]].get(name, []))
            if s:
                lines.append(f"| {name} | {s[0]:.2f} | {s[1]:.0f} |")
            else:
                lines.append(f"| {name} | - | - |")
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
    for label, tgfx_dir in CONFIGS:
        abs_dir = os.path.normpath(os.path.join(bench_base.PROJECT_ROOT, tgfx_dir))
        if not os.path.isdir(abs_dir):
            print(f"[{label}] tgfx 路径不存在，跳过: {abs_dir}", flush=True)
            continue
        results = bench_base.run_scenario(label, tgfx_dir, BENCH_NAMES, RUNS)
        if results is None:
            print(f"[{label}] 构建失败，跳过", flush=True)
            continue
        results_by_label[label] = results
    if not results_by_label:
        print("没有任何配置产生数据", flush=True)
        sys.exit(1)
    write_report(results_by_label, BENCH_NAMES)
    # 报告写完后清理原始日志，报告为唯一留档
    runs_root = os.path.join(bench_base.PROJECT_ROOT, "runs")
    for label in results_by_label:
        shutil.rmtree(os.path.join(runs_root, label), ignore_errors=True)
    print("原始日志已清理: " + runs_root, flush=True)


if __name__ == "__main__":
    main()
