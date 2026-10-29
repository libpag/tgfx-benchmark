# LayerBench 阴影样式性能测试报告

## 1 背景

对比 tgfx Layer 模块阴影样式（DropShadowStyle / InnerShadowStyle）优化前后的性能差异。

- 测试对象：`python3 tools/bench_shadow.py`（用例：LayerBench-DropShadow、LayerBench-InnerShadow）
- 统计方式：每个用例每配置连续运行 5 轮；进程达到稳定（isMaxDrawCountReached）后进入 5 秒采样窗口，fps 取窗口内采样均值，count 取窗口内最后一次 drawCount
- 用例参数：SolidLayer 粒子，尺寸 4~14 x density 随机，圆角按索引交替 0 / 宽 x 0.25；阴影 blur = 短边 x 1/4，offset = 短边 x 1/4，spread = 短边 x 1/8，颜色 (1,0,0,0.5)
- 原始日志：runs/<label>/<用例名>/run<N>.log

## 2 结果汇总（平均值）

| 用例 | baseline fps | candidate fps | fps 变化 | baseline count | candidate count | count 变化 |
|---|---:|---:|---:|---:|---:|---:|
| LayerBench-DropShadow | 56.37 | 59.23 | +2.87 (+5.08%) | 173 | 433 | +259.60 (+149.88%) |
| LayerBench-InnerShadow | 56.11 | 58.94 | +2.84 (+5.05%) | 141 | 375 | +234.20 (+166.10%) |

RenderTarget 指标（每配置每用例 5 轮均值；rtAlive = 稳定时存活的 RenderTarget 数（含缓存中未使用的实例），每帧新建 = 稳定期每帧新建的 RenderTarget 数）：

| 用例 | baseline<br>rtAlive | candidate<br>rtAlive | rtAlive<br>变化 | baseline<br>rtAlive/图层 | candidate<br>rtAlive/图层 | rtAlive/图层<br>变化 | baseline<br>每帧新建 | candidate<br>每帧新建 | 每帧新建<br>变化 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| LayerBench-DropShadow | 908 | 743 | -165.00 (-18.17%) | 5.428 | 1.675 | -3.75 (-69.14%) | 3.20 | 3.12 | -0.08 (-2.50%) |
| LayerBench-InnerShadow | 1205 | 1021 | -184.00 (-15.27%) | 8.992 | 2.764 | -6.23 (-69.26%) | 3.23 | 3.25 | +0.02 (+0.62%) |

## 3 详细数据

### 3.1 baseline（TGFX_DIR=../tgfx）

#### LayerBench-DropShadow

- Run1: fps=58.04, count=169
- Run2: fps=57.56, count=172
- Run3: fps=56.04, count=175
- Run4: fps=55.65, count=173
- Run5: fps=54.57, count=177
- 平均：fps=56.37（min=54.57, max=58.04）, count=173

#### LayerBench-InnerShadow

- Run1: fps=56.43, count=139
- Run2: fps=55.17, count=143
- Run3: fps=54.68, count=142
- Run4: fps=58.81, count=136
- Run5: fps=55.44, count=145
- 平均：fps=56.11（min=54.68, max=58.81）, count=141

### 3.2 candidate（TGFX_DIR=../TGFX_WT_2）

#### LayerBench-DropShadow

- Run1: fps=58.70, count=386
- Run2: fps=59.31, count=439
- Run3: fps=59.67, count=437
- Run4: fps=59.42, count=445
- Run5: fps=59.07, count=457
- 平均：fps=59.23（min=58.70, max=59.67）, count=433

#### LayerBench-InnerShadow

- Run1: fps=57.96, count=379
- Run2: fps=59.36, count=367
- Run3: fps=58.90, count=381
- Run4: fps=59.54, count=367
- Run5: fps=58.94, count=382
- 平均：fps=58.94（min=57.96, max=59.54）, count=375
