---
name: bench-report
description: 在 tgfx-benchmark 仓库中执行性能对比测试并生成报告。当用户要求测量某个渲染场景/优化的性能（基线测量、优化前后对照）时使用。用户需提供：被测用例名列表、baseline 与 candidate 的 tgfx 路径（或只测基线）、轮数（默认 5）、报告文件名（默认放 reports/）。
---

# bench-report：tgfx-benchmark 性能测试与报告生成

## 适用场景

用户在 tgfx-benchmark 仓库要求做性能测量或优化前后对比。基础设施已就绪：

- 原生端通过 BENCH_NAME 环境变量按名选择用例（src/platform/mac/TGFXWindow.mm），
  用例稳定后周期性打印 PERF 行（name/fps/drawCount/stable）。
- tools/bench_base.py 是公共驱动：独立构建目录（cmake-build-bench-<label>）、
  进程生命周期、stable 等待 + 5 秒采样窗口、多轮循环与失败终止、avg/min/max 统计。
- 场景脚本（如 tools/run_shadow.py）只需声明 CONFIGS（label + tgfx 路径）、
  BENCH_NAMES、RUNS、REPORT_PATH。

## 输入

向用户确认以下参数（缺省值可直接采用）：

1. 被测用例名列表：必须已在 src/base/Bench.cpp 的 drawers 表中注册；未注册先注册并编译验证。
2. 对照配置：baseline 与 candidate 各自的 tgfx 路径（相对项目根目录，如 ../tgfx）。
   只测基线时只给一个。路径不存在的配置会被跳过。
3. 轮数：默认 5。
4. 报告路径：默认 reports/PERF_<场景名>.md。

## 执行流程

1. 确认用例已注册；按需修改 src/base/Bench.cpp，只保留被测用例（其余注释）。
2. 若现有场景脚本不满足需求，以 run_shadow.py 为模板新建 tools/run_<场景>.py，
   只改 CONFIGS / BENCH_NAMES / RUNS / REPORT_PATH，公共协议不改。
3. 后台运行 python3 tools/run_<场景>.py（脚本会自动配置构建目录、编译、逐轮运行）。
   全量构建加多轮运行耗时数分钟到十几分钟，用 run_in_background 并轮询进度。
4. 运行期间提醒用户：会弹出真实 GUI 窗口，保持窗口不被遮挡、系统负载稳定。
5. 脚本结束后检查报告与 runs/<label>/ 下的原始日志是否齐全。

## 报告纪律（遵循 PerfTest 规范）

- 报告结构：背景 → 测试方案（用例参数口径、判定协议、轮数、日志归档位置）→
  测试数据（每配置一小节：逐轮明细表 + avg/min/max 汇总）→ 对比（仅双配置时）。
- 对比表"变化"列统一写 绝对值（百分率），不用约数。
- 逐轮明细与原始日志必须可从 runs/<label>/<用例名>/run<N>.log 复算。
- 某轮失败在明细中标注失败原因，汇总只基于有效轮并注明有效轮数。
- 被测方法变更后必须重测并重建汇总表，不混用新旧口径的数据。
- 连续 3 轮失败脚本会提前终止；此时先排查原因（构建问题/窗口被遮挡/用例未注册），
  不要直接出报告。
