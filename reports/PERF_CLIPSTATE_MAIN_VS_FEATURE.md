# ClipState 性能测试文档（Main vs Feature）

## 1 背景

本次测试用于对比裁剪区优化前后在 `ParticleBench-Rect` 场景下的性能差异。

- 测试对象：`python3 tools/run_bench.py ParticleBench-Rect`
- 统计方式：每个 case 连续运行 10 轮，使用脚本输出的平均值（`fps`、`count`）
- 分支顺序：先测 Main，再切换 TGFX 到 Feature 分支后复测
- 数据规则：如果某个 case 被要求“重新测试”，则废弃第一次数据，只保留第二次数据

> 按上述规则，本报告中 Feature 分支的 `None` 和 `Path` 使用的是复测后的第二次数据。

---

## 2 结果汇总（平均值）

| ClipState | Main 实现方式 | Feature 实现方式 | Main FPS | Feature FPS | FPS 变化 (Feature-Main) | Main Count | Feature Count | Count 变化 (Feature-Main) |
|---|---|---|---:|---:|---:|---:|---:|---:|
| None | 无 | None | 59.06 | 59.24 | +0.18 (+0.30%) | 647595 | 634096 | -13499 (-2.08%) |
| RectAA | 矩形解析 FP | 矩形解析 FP | 53.64 | 54.54 | +0.90 (+1.68%) | 6984 | 7448 | +464 (+6.64%) |
| RectNonAA | GPU 硬件裁剪 | 硬件裁剪 | 53.65 | 51.48 | -2.17 (-4.04%) | 7873 | 8301 | +428 (+5.44%) |
| MatrixRect | 覆盖率纹理 | 矩形解析 FP | 28.65 | 55.54 | +26.89 (+93.86%) | 1078 | 7419 | +6341 (+588.22%) |
| MatrixRRect | 覆盖率纹理 | 圆角矩形解析 FP | 21.83 | 52.60 | +30.77 (+140.95%) | 1476 | 6267 | +4791 (+324.59%) |
| Path | 覆盖率纹理 | 覆盖率纹理 | 25.24 | 25.82 | +0.58 (+2.30%) | 1298 | 1196 | -102 (-7.86%) |

---

## 3 详细数据

### 3.1 Main 分支

#### None
- Run1: fps=59.29, count=651036
- Run2: fps=59.04, count=654213
- Run3: fps=58.87, count=643779
- Run4: fps=59.50, count=667957
- Run5: fps=59.02, count=650888
- Run6: fps=59.42, count=656288
- Run7: fps=59.30, count=660868
- Run8: fps=58.70, count=596574
- Run9: fps=58.10, count=648003
- Run10: fps=59.38, count=646345
- 平均：fps=59.06, count=647595

#### RectAA
- Run1: fps=49.20, count=6867
- Run2: fps=54.11, count=6771
- Run3: fps=56.24, count=7103
- Run4: fps=55.01, count=7051
- Run5: fps=53.60, count=6995
- Run6: fps=56.60, count=6994
- Run7: fps=53.42, count=6936
- Run8: fps=54.53, count=7085
- Run9: fps=52.10, count=7034
- Run10: fps=51.59, count=7005
- 平均：fps=53.64, count=6984

#### RectNonAA
- Run1: fps=50.60, count=8143
- Run2: fps=51.08, count=7848
- Run3: fps=55.58, count=7865
- Run4: fps=53.93, count=7781
- Run5: fps=52.20, count=7975
- Run6: fps=54.59, count=7820
- Run7: fps=54.95, count=7895
- Run8: fps=54.56, count=7847
- Run9: fps=55.34, count=7618
- Run10: fps=53.65, count=7935
- 平均：fps=53.65, count=7873

#### MatrixRect
- Run1: fps=19.62, count=1455
- Run2: fps=36.66, count=748
- Run3: fps=28.37, count=992
- Run4: fps=40.02, count=659
- Run5: fps=20.52, count=1385
- Run6: fps=21.96, count=1351
- Run7: fps=40.03, count=650
- Run8: fps=37.99, count=695
- Run9: fps=17.20, count=1634
- Run10: fps=24.11, count=1215
- 平均：fps=28.65, count=1078

#### MatrixRRect
- Run1: fps=34.90, count=784
- Run2: fps=27.36, count=1047
- Run3: fps=16.17, count=1807
- Run4: fps=22.44, count=1352
- Run5: fps=34.28, count=806
- Run6: fps=15.53, count=1876
- Run7: fps=15.02, count=1918
- Run8: fps=21.80, count=1395
- Run9: fps=14.80, count=1962
- Run10: fps=15.97, count=1817
- 平均：fps=21.83, count=1476

#### Path
- Run1: fps=44.24, count=729
- Run2: fps=29.94, count=960
- Run3: fps=16.04, count=1766
- Run4: fps=23.42, count=1261
- Run5: fps=15.29, count=1874
- Run6: fps=15.03, count=1878
- Run7: fps=31.83, count=866
- Run8: fps=31.57, count=884
- Run9: fps=29.82, count=929
- Run10: fps=15.23, count=1832
- 平均：fps=25.24, count=1298

### 3.2 Feature 分支（复测替换规则后）

#### None（使用复测后的第二次数据）
- Run1: fps=58.98, count=630790
- Run2: fps=59.49, count=632241
- Run3: fps=59.48, count=634339
- Run4: fps=58.97, count=630117
- Run5: fps=59.47, count=638404
- Run6: fps=59.36, count=620180
- Run7: fps=59.39, count=647373
- Run8: fps=59.15, count=655226
- Run9: fps=58.86, count=619911
- Run10: fps=59.28, count=632380
- 平均：fps=59.24, count=634096

#### RectAA
- Run1: fps=50.78, count=7548
- Run2: fps=54.11, count=7421
- Run3: fps=56.21, count=7348
- Run4: fps=54.80, count=7606
- Run5: fps=54.81, count=7406
- Run6: fps=55.32, count=7411
- Run7: fps=54.50, count=7393
- Run8: fps=54.11, count=7533
- Run9: fps=54.37, count=7378
- Run10: fps=56.40, count=7441
- 平均：fps=54.54, count=7448

#### RectNonAA
- Run1: fps=49.62, count=8234
- Run2: fps=51.76, count=8468
- Run3: fps=51.03, count=8218
- Run4: fps=52.09, count=8305
- Run5: fps=52.38, count=8213
- Run6: fps=51.63, count=8370
- Run7: fps=50.86, count=8404
- Run8: fps=53.78, count=8242
- Run9: fps=51.01, count=8274
- Run10: fps=50.63, count=8283
- 平均：fps=51.48, count=8301

#### MatrixRect
- Run1: fps=54.73, count=7541
- Run2: fps=54.18, count=7399
- Run3: fps=56.22, count=7429
- Run4: fps=57.24, count=7374
- Run5: fps=55.88, count=7354
- Run6: fps=55.15, count=7406
- Run7: fps=55.02, count=7337
- Run8: fps=56.14, count=7504
- Run9: fps=54.20, count=7392
- Run10: fps=56.61, count=7451
- 平均：fps=55.54, count=7419

#### MatrixRRect
- Run1: fps=48.79, count=6275
- Run2: fps=54.07, count=6191
- Run3: fps=53.01, count=6269
- Run4: fps=52.62, count=6379
- Run5: fps=52.69, count=6302
- Run6: fps=53.87, count=6212
- Run7: fps=53.04, count=6276
- Run8: fps=53.51, count=6219
- Run9: fps=51.90, count=6251
- Run10: fps=52.51, count=6295
- 平均：fps=52.60, count=6267

#### Path（使用复测后的第二次数据）
- Run1: fps=30.39, count=934
- Run2: fps=31.32, count=908
- Run3: fps=27.34, count=1042
- Run4: fps=36.27, count=773
- Run5: fps=19.36, count=1463
- Run6: fps=15.56, count=1808
- Run7: fps=17.01, count=1651
- Run8: fps=29.40, count=960
- Run9: fps=33.97, count=817
- Run10: fps=17.56, count=1607
- 平均：fps=25.82, count=1196
