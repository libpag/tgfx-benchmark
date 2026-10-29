---
name: bench-report
description: 在 tgfx-benchmark 仓库中执行性能对比测试并生成报告。当用户要求测量某个渲染场景/优化的性能（基线测量、优化前后对照）时使用。用户需提供：被测用例名列表、baseline 与 candidate 的 tgfx 路径（或只测基线）、轮数（默认 5）、报告文件名（默认放 reports/）。
---

# bench-report：tgfx-benchmark 性能测试与报告生成

## 适用场景

用户在 tgfx-benchmark 仓库要求做性能测量或优化前后对比。基础设施已就绪：

- 原生端通过 BENCH_NAME 环境变量按名选择用例（src/platform/mac/TGFXWindow.mm），
  用例稳定后周期性打印 PERF 行（name/fps/drawCount/stable），此为常驻能力。
- RenderTarget 类探针指标（rtCreated/rtAlive，每帧 PROBE 行）默认不存在，需要时
  按"探针生命周期"一节临时添加探针后再测量。
- 通用脚本在本 skill 的 scripts/ 目录：bench_base.py（公共驱动：独立构建目录
  cmake-build-bench-<label>、进程生命周期、stable 等待 + 5 秒采样窗口、多轮循环
  （自带轮间冷却）与失败终止、avg/min/max 统计）、gen_perf_report.py（从归档日志
  重建报告，不重跑测试，参数为场景脚本路径）。
- 场景脚本与报告放在 test/<场景>/ 目录（如 test/shadow/bench_shadow.py 与
  test/shadow/PERF_LAYER_SHADOW.md）。场景脚本声明 CONFIGS（label + tgfx 路径）、
  BENCH_NAMES、RUNS、REPORT_PATH（指向同目录报告文件），并定义 write_report。

## 输入

向用户确认以下参数（缺省值可直接采用）：

1. 被测用例名列表：必须已在 src/base/Bench.cpp 的 drawers 表中注册；未注册先注册并编译验证。
2. 对照配置：baseline 与 candidate 各自的 tgfx 路径（相对项目根目录，如 ../tgfx）。
   只测基线时只给一个。路径不存在的配置会被跳过。
3. 轮数：默认 5。
4. 报告路径：默认 test/<场景>/PERF_<场景名>.md（与场景脚本同目录）。

## 执行流程

1. 确认用例已注册；按需修改 src/base/Bench.cpp，只保留被测用例（其余注释）。
2. 若现有场景脚本不满足需求，新建 test/<场景>/ 目录，以 test/shadow/bench_shadow.py
   为模板创建 bench_<场景>.py（如 test/clip/bench_clip.py），只改 CONFIGS / BENCH_NAMES /
   RUNS / REPORT_PATH / write_report，公共协议不改。
3. 后台运行 python3 test/<场景>/bench_<场景>.py（脚本会自动配置构建目录、编译、逐轮运行）。
   全量构建加多轮运行耗时数分钟到十几分钟，用 run_in_background 并轮询进度。
4. 运行期间提醒用户：会弹出真实 GUI 窗口，保持窗口不被遮挡、系统负载稳定。
5. 脚本结束后检查报告内容是否齐全（汇总表、逐轮明细、口径说明）。

## 测量纪律

- 任何逐轮循环都必须有轮间冷却：调用 bench_base.run_one 的自写循环要在每轮之间
  sleep bench_base.COOLDOWN_SEC，等被终止进程的 GPU 资源（离屏纹理等）被系统完全回收
  后再启动下一轮；优先直接用 bench_base.run_bench_name，它自带冷却。
- 多类指标同源同轮次：PERF 与 PROBE 行在同一份 stdout 日志里，一次运行同时产出，
  从 run_bench_name 产出的 run<N>.log 一并解析。不拆成两个脚本各跑一遍，
  不引入第二个数据源，脚本间也不通过中间文件（如 rt_probe.json）传递数据。
- 单轮数据只能用于机制分析，不能进汇总表；进汇总表的指标必须跑满声明的轮数。

## 报告规范

- 报告结构：背景 → 测试方案（用例参数口径、判定协议、轮数、日志归档位置）→
  测试数据（每配置一小节：逐轮明细表 + avg/min/max 汇总）→ 对比（仅双配置时）。
- 汇总表按指标维度拆分，一个维度一张表：fps/count 一张表；RenderTarget 这类探针指标
  单独一张表。不把不同来源/不同口径的指标合并进同一张宽表。
- 每张指标表必须包含该维度下所有已测指标：例如 RT 维度同时测了存活（rtAlive、
  rtAlive/图层）和创建（每帧新建），三列都要进表，不挑其中一部分。
- 指标表前用一句话给口径（测量方式、轮数、指标定义），方案小节不重复这段说明。
- 对比表"变化"列统一写 绝对值（百分率），不用约数。
- 逐轮明细写进报告正文（报告为唯一留档，日志随即删除）。
- 某轮失败在明细中标注失败原因，汇总只基于有效轮并注明有效轮数。
- 被测方法变更后必须重测并重建汇总表，不混用新旧口径的数据。
- 连续 3 轮失败脚本会提前终止；此时先排查原因（构建问题/窗口被遮挡/用例未注册），
  不要直接出报告。

## 探针生命周期

- 探针（计数器、额外日志行等测量专用改动）只存在于测量期间，代码里统一标 [Probe]，
  测量完成后立即从所有工作树还原（两棵 tgfx 树和 benchmark 仓库都要还原），
  不留在待提交代码里，也不存档补丁——再次需要时重新编写。
- 探针典型形态：tgfx 侧在目标资源的创建/析构处加计数器并以 extern "C" 导出；
  benchmark 侧在 TGFXWindow.mm 每帧打印 PROBE 行（字段名=累计值/存活值）。
  bench_shadow.py 的 PROBE 解析在没有探针数据时自动跳过，报告自动省略对应表格。

## 日志生命周期

- runs/<label>/ 下的逐轮日志是临时文件：数据归纳进报告后立即删除（bench_shadow.py
  已在报告写完后自动清理），报告是唯一留档。逐轮明细以报告内的列表为准。
- 手动/临时运行产生的日志（如机制验证用的 probe 日志）同样在归纳后删除，不留存仓库。

