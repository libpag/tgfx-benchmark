# 基准测试目的需求文档

日期：2026-10-28

状态：final，可进入修改方案与分阶段实施

## 1. 背景

当前 benchmark 把 TGFX 首帧、预热和稳态容量混在同一条测量路径中。预热只按
墙钟时间结束，因此首帧耗时超过预热时间时，shader/pipeline 编译、资源创建和
Surface 初始化可能进入稳态样本。

需要新增独立的 `testIntent`：

- `cold-start`：测量 TGFX 首次渲染能力；
- `steady-state`：测量缓存和资源稳定后的持续渲染能力。

两种测试目的共用场景、后端、codegen 和构建包体系，但测量窗口、指标和报告
必须分开。

## 2. 已确定的产品决策

### 2.1 维度与覆盖

- `testIntent` 与 `runMode` 是两个独立维度。
- `cold-start` 仅支持 fixed-count，不支持 warmup 和 ramp。
- `steady-state` 保留当前 manual/fixed/ramp 三条路径。
- 以下四种组合全部开放：
  - AOT + WebGL
  - AOT + WebGPU
  - JIT + WebGL
  - JIT + WebGPU
- UI、后台 runner、live stats 和报告都需要识别 `testIntent`。
- candidate package 也应能在后台 runner 中选择 cold-start 或 steady-state。

### 2.2 Cold-start 计时边界

选择 TGFX 首次渲染口径，不测完整页面加载。

```text
package ready
  -> 选择 scene
  -> 一次性设置 initial count
  -> 公共字体和图片已就绪
  -> 标记 cold-start 起点
  -> 第一次 draw
  -> 第一次 submit
  -> 第一次可见
  -> 再采集 30 个初始帧
  -> 自动结束
```

计时不包含：

- 页面导航；
- Wasm 下载和实例化；
- benchmark package 初始化；
- 公共字体和图片预加载。

计时包含：

- 第一次场景 draw；
- TGFX 首次 flush/submit；
- 首次可见前的浏览器展示阶段；
- 首帧后的 30 个辅助帧。

### 2.3 Cold-start workload

- 首版新增一个专用 `coldstart.shader-mix` 复杂场景。
- 场景覆盖多种常用 shader，而不是从现有 MotionMark 场景中任选部分组合。
- 现有图片、字体、颜色和绘制分组可以复用，但不得为了复用而改变目标测试覆盖。
- 初始 workload 必须明显高于现有复杂场景的常用负载；该场景每个 count 单位
  包含全部 shader 组，首版默认 256 个单位，最终值由首帧基线校准。
- initial count 必须在 cold-start 起点之前设置完成。
- cold-start 期间不得增加、回撤或按时间渐变对象数量。
- 首版自动验收只跑 cold-start 首帧路径，不要求跑完整稳态容量矩阵。

### 2.4 单次运行与缓存隔离

- 一次 UI 点击或一次 runner 调用只执行一次 benchmark，只生成一份报告。
- benchmark 内部不再提供自动 repeat。
- 连续测试由用户或外部编排器逐次触发。
- 每次运行都必须显式隔离应用状态和可控制的资源缓存。
- 相邻两次运行不能依赖上一次的页面、TGFX context 或资源实例。
- steady-state 的相邻运行也必须使用同样的缓存隔离策略。

### 2.5 Steady-state 预热门槛

steady-state 的 fixed/ramp 判定、窗口、容差和报告语义保持不变，但不再提供
用户可配置的墙钟预热时长。所有稳态模式固定丢弃启动帧：

```text
完成至少 10 个成功 draw/flush 帧
```

从之后的下一帧开始正式测量或启动 adaptive 搜索。cold-start 不丢弃这些帧。

### 2.6 AOT/JIT 对比边界

- 不对 package 来源、分支基线或版本相近程度做严格验证。
- 用户选择什么 package，就测试什么 package。
- 用户选择哪两个 package，就对比哪两个 package。
- 报告记录 package ID、commit、artifact hash 和运行身份，但不因 commit
  不同拒绝运行。
- 结论文字必须明确写成“所选 AOT package 与所选 JIT package 的对比”，不能
  自动声称差异完全来自 AOT codegen。

### 2.7 UI 与报告适配

- UI 增加“首帧测试 / 稳态测试”目的选择。
- cold-start 下隐藏 warmup、measurement、target FPS 和 ramp 控件。
- cold-start 下保留 scene、count、backend、AOT/JIT package 选择。
- cold-start 只展示支持首帧测试的场景。
- steady-state 保持现有 fixed/ramp UI 结构。
- cold-start 和 steady-state 使用不同的报告内容及展示结构。
- live stats 必须包含 `testIntent`，避免父页面用稳态逻辑解释首帧运行。
- 旧报告缺少 `testIntent` 时按 steady-state 导入。

## 3. 维度模型

```text
testIntent:
  cold-start
  steady-state

runMode:
  cold-start:   fixed-count only
  steady-state: fixed-count | ramp | manual

engineMode:
  aot
  jit

backend:
  webgl
  webgpu
```

公共维度：

- scene；
- count；
- viewport 与 DPR；
- browser、platform、backend；
- package identity 与 artifact hash；
- AOT runtime evidence；
- run nonce 和 cache policy。

## 4. 专用首帧场景需求

首版新增稳定 ID：

```text
coldstart.shader-mix
```

场景必须以确定性方式在同一首帧中覆盖以下常用路径：

- solid fill；
- antialiased rect/rrect/oval；
- stroke 和 hairline line；
- linear gradient；
- radial gradient；
- image texture，包含缩放、旋转和 alpha；
- text atlas；
- mask 或 blend 中至少一种常用合成路径。

设计要求：

- 每个 count 单位包含同一组必选绘制路径，新增单位必须同时增加所有组；
- 所有绘制对象必须在画布范围内，不得通过离屏对象制造无效容量；
- 允许复用现有 bridge/MotionMark 图片、注册字体、颜色和已有绘制 helper；
- 不复制图片、字体或生成新的重复资源；
- 首帧后 30 帧保持确定性动画，动画边界不得离开各自网格单元；
- report/live stats 必须能识别 `coldstart.shader-mix`、scene version 和 count；
- 自动覆盖测试至少验证包加载、非空绘制、AOT hit 数和首帧完成。

`count=256` 作为首版默认值；完成第一轮基线后可以校准，但不得为凸显 AOT 而
在没有记录的情况下临时改值。

## 5. Cold-start 数据与状态需求

### 5.1 起止状态

- `coldStartBeginUs`：scene、count 和公共资源就绪后，第一次 draw 前。
- `firstSubmitUs`：coldStartBegin 到第一次提交完成。
- `firstVisibleUs`：coldStartBegin 到下一次 rAF 确认首帧可见。
- `initialWindowEndFrame`：首次可见后的第 30 帧。

### 5.2 首帧指标

- `firstFrameCallbackUs`；
- `firstFrameSceneDrawUs`；
- `firstFrameFlushUs`；
- `firstFrameSubmitUs`；
- `firstSubmitUs`；
- `firstVisibleUs`；
- `initialCount`；
- AOT runtime evidence。

### 5.3 初始 30 帧辅助指标

辅助窗口只用于观察首帧后是否立即出现长时间抖动，不作为稳态容量：

- 帧数；
- callback interval p50/p95/max；
- scene draw p50/p95；
- flush p50/p95；
- submit p50/p95；
- 长帧数量。

报告必须明确标记为 `initial-window`，不得输出 `steadyCapacity`。

## 6. 运行状态需求

- cold-start 不创建 warmup 样本。
- cold-start 不执行 ramp。
- cold-start 不调用 adaptive controller。
- cold-start 完成后 `executionStatus` 为 `completed`。
- `count` 字段表示已经生效的实际 initial count。
- 若场景拒绝或裁剪 count，报告记录 requested 和 actual count。
- 如果 AOT package 未加载 bundle，cold-start 结果仍可保存，但必须标记为
  非有效 AOT 结果。

## 7. Live Stats 需求

公共字段增加：

- `testIntent`；
- `runMode`；
- `phase`；
- `initialCount`；
- `coldStartElapsedUs`；
- `firstSubmitUs`；
- `firstVisibleUs`；
- `initialWindowFrameCount`。

父页面必须根据 `testIntent` 选择不同轮询和结束条件：

- cold-start：首帧 + 30 帧后自动结束；
- steady-state：保持现有 fixed/ramp 逻辑。

## 8. 报告需求

### 8.1 Cold-start 报告

首屏重点展示：

- 首次 submit；
- 首次可见；
- 首帧 callback；
- scene draw / flush / submit；
- initial count；
- AOT/JIT、WebGL/WebGPU 和 package 身份；
- AOT runtime validity。

次级区域展示初始 30 帧曲线和阶段分布。

Cold-start 报告不展示稳态容量、目标 FPS 或 ramp 收敛结论。

### 8.2 Steady-state 报告

继续展示：

- warmup actual；
- FPS；
- frame interval p50/p95/p99/max；
- adaptive/ramp 结果；
- 稳态容量和失效原因。

报告增加 `testIntent=steady-state`，但现有指标含义不变。

### 8.3 历史记录与导入

- 历史记录同时保留 cold-start 和 steady-state。
- 历史表必须显示测试目的。
- 导入旧报告时，缺省识别为 `steady-state`。
- 不同 testIntent 的报告不得自动计算 delta 或排名。

## 9. 缓存与运行隔离需求

每次运行至少执行：

- 创建新的 benchmark iframe/page；
- 生成唯一 `runNonce`；
- 所有可控制的 package/asset URL 带 nonce；
- benchmark server 对运行资源发送 `Cache-Control: no-store`；
- 不沿用上一次的 scene、count、视图或 benchmark state；
- 后台 runner 每次运行使用新的 browser context。

隔离范围明确为：

- UI 保证 DOM、JS、TGFX Context、场景状态和资源实例不跨 run 复用；
- UI 保证可控制的 HTML/JS/Wasm/图片/字体请求不使用上一次响应缓存；
- 后台 runner 每次运行使用新 browser context；cold-start 自动验收使用新
  browser process 和临时 user-data-dir；
- 浏览器 GPU shader disk cache 和 OS/GPU 驱动级缓存不能由页面可靠清除，
  不计入 UI 的强隔离承诺，但必须写入报告；
- 相邻 run 的隔离等级分为 `context+assets` 和 `fresh-process`。

## 10. 验收标准

- 四种 AOT/JIT × WebGL/WebGPU 组合都能选择 cold-start。
- cold-start 只显示允许的复杂场景。
- initial count 在第一次 draw 前已经完整生效。
- cold-start 不产生 warmup 和 ramp。
- 一次操作只运行一次并生成一份报告。
- 首次 submit、首次可见、首帧阶段和初始 30 帧都可在 UI 查看。
- 四种组合同样可选择 steady-state，原 fixed/ramp 可用。
- steady-state 固定丢弃前 10 个成功 draw/flush 帧。
- 相邻两次 run 不共享应用状态和可控制的资源缓存。
- 所有 live stats 和报告都包含 `testIntent`。
- AOT package 运行时可记录 AOT hit/miss/fallback 和有效性。
- 自动验收只跑 cold-start 代表场景的首帧路径。

## 11. 已确认的首版实现参数

- cold-start scene：`coldstart.shader-mix`；
- cold-start 默认 count：`256`，允许 UI 和 runner 显式覆盖；
- cold-start 首帧后辅助采样：`30` 帧；
- steady-state 启动隔离：丢弃前 `10` 个成功 draw/flush 帧；
- 单次调用只执行一个 benchmark，不自动 repeat；
- UI 隔离等级：`context+assets`；
- cold-start 自动验收隔离等级：`fresh-process`；
- cold-start 自动验收只跑 `coldstart.shader-mix` 首帧；
- AOT/JIT 不进行来源和版本相近性强制校验。

## 12. 非目标

- 不测完整页面导航到首屏。
- 不引入 GPU timestamp 或 GPU pass 级计时。
- 不修改现有 MotionMark 场景视觉内容，只新增独立 cold-start 场景。
- 不自动验证 AOT/JIT package 的分支基线或来源。
- 不为所有场景默认生成大负载压力 count。
- 不在 benchmark 内自动重复同一 case。
- 不把首帧和稳态计算成一个综合分数。
