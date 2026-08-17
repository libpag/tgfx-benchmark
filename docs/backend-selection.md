# Benchmark GPU 后端选择方案

## 目标

为 TGFX Benchmark 提供统一、互斥的编译期 GPU 后端选择入口。每个构建目录只包含一个 GPU 后端，避免多个 `TGFX_USE_*` 缓存变量组合导致实际后端不明确。

统一参数：

```text
BENCHMARK_BACKEND=AUTO|OPENGL|ANGLE|METAL|VULKAN|D3D12|WEBGL|WEBGPU
```

`AUTO` 保持现有默认行为：

- Windows：OpenGL/WGL
- macOS：OpenGL/CGL
- Web：WebGL

为避免 CMake 缓存污染，不同后端必须使用不同构建目录。

## 当前实施范围

| 平台 | 可选后端 | 状态 |
|---|---|---|
| Windows | `OPENGL`、`ANGLE`、`VULKAN`、`D3D12` | 本次实施 |
| macOS | `OPENGL`、`METAL` | 本次实施 |
| Web/Emscripten | `WEBGL`、`WEBGPU` | 本次实施 |

当前 Benchmark 没有 Linux、Android、iOS 应用入口，因此以下组合不在本次范围内：

- Linux Vulkan
- Android Vulkan
- iOS Metal
- macOS Vulkan/MoltenVK
- Native Dawn WebGPU

其中，当前 TGFX 的 `VulkanWindow` 没有 macOS 窗口创建接口；`WebGPUWindow` 的 Canvas 创建路径面向 Emscripten，不能直接作为 Native Dawn 窗口使用。

## 设计

### CMake 映射

根工程通过 `cmake/BenchmarkBackend.cmake` 完成：

1. 解析并规范化 `BENCHMARK_BACKEND`。
2. 在加入 TGFX 子目录前校验平台支持矩阵。
3. 检测并拒绝旧的 TGFX 后端缓存选项。
4. 将选定值映射到一个 TGFX 后端。
5. 向 Benchmark 目标传递 `BENCHMARK_BACKEND_<NAME>` 编译宏。

映射关系：

| Benchmark 后端 | TGFX 配置 |
|---|---|
| `OPENGL` | `TGFX_USE_OPENGL=ON` |
| `ANGLE` | `TGFX_USE_OPENGL=ON`、`TGFX_USE_ANGLE=ON` |
| `METAL` | `TGFX_USE_METAL=ON` |
| `VULKAN` | `TGFX_USE_VULKAN=ON` |
| `D3D12` | `TGFX_USE_D3D12=ON` |
| `WEBGL` | `TGFX_USE_OPENGL=ON` |
| `WEBGPU` | `TGFX_USE_WEBGPU=ON` |

Benchmark 只接受 `BENCHMARK_BACKEND` 作为后端选择入口，不兼容 `-DTGFX_USE_ANGLE=ON` 等旧的 TGFX 后端开关。若传入 `TGFX_USE_*`，或显式启用 `TGFX_D3D12_USE_WARP`，配置阶段会直接报错，避免后端选择不明确。

### 平台窗口

绘制路径统一使用：

```text
Window -> Device -> Context -> Surface -> Canvas -> Recording -> submit
```

平台代码只在创建 `tgfx::Window` 时区分后端：

- Windows：`WGLWindow`、`EGLWindow`、`VulkanWindow`、`D3D12Window`
- macOS：`CGLWindow`、`MetalWindow`
- Web：`WebGLWindow`、`WebGPUWindow`

`Surface`、绘制、Recording 提交和帧统计保持共用。

## 构建示例

### Windows

在 Visual Studio Native Tools Command Prompt 中执行：

```bat
cmake -S . -B out/win-opengl -G "Visual Studio 16 2019" -A x64 -DBENCHMARK_BACKEND=OPENGL
cmake -S . -B out/win-angle -G "Visual Studio 16 2019" -A x64 -DBENCHMARK_BACKEND=ANGLE
cmake -S . -B out/win-vulkan -G "Visual Studio 16 2019" -A x64 -DBENCHMARK_BACKEND=VULKAN
cmake -S . -B out/win-d3d12 -G "Visual Studio 16 2019" -A x64 -DBENCHMARK_BACKEND=D3D12
cmake --build out/win-d3d12 --config Release --target Benchmark
```

ANGLE 构建完成后会把 `libEGL.dll` 和 `libGLESv2.dll` 复制到 `Benchmark.exe` 所在目录。Vulkan 性能测试需要安装可用的硬件驱动和 Vulkan Loader。

### macOS

```bash
cmake -S . -B out/mac-opengl -DBENCHMARK_BACKEND=OPENGL -DCMAKE_BUILD_TYPE=Release
cmake -S . -B out/mac-metal -DBENCHMARK_BACKEND=METAL -DCMAKE_BUILD_TYPE=Release
cmake --build out/mac-metal --target Benchmark
```

### Web

Web 构建沿用仓库现有的 npm 入口。首次使用先安装依赖：

```bash
cd web
npm install
```

构建和运行多线程版本：

```bash
# WebGL
npm run build
npm run server

# WebGPU
npm run build:webgpu
npm run server:webgpu
```

单线程版本使用：

```bash
npm run build:st
npm run server:st
npm run build:webgpu:st
npm run server:webgpu:st
```

`web/script/cmake.demo.js` 会把 npm 后端选择转换为 `BENCHMARK_BACKEND=WEBGL` 或 `BENCHMARK_BACKEND=WEBGPU`，无需手动执行 `emcmake cmake`。构建脚本会安装并启用 Emscripten `4.0.15`；WebGPU 使用该版本的 `-sUSE_WEBGPU=1` 和 `WebGPU` runtime method。浏览器必须支持 WebGPU，并通过 `localhost` 或 HTTPS 访问。

## 验证要求

每个后端至少执行：

1. CMake 配置成功，日志中的 TGFX 后端与指定值一致。
2. Release 构建成功。
3. 主窗口或 Web Canvas 可持续绘制。
4. 四种 Benchmark case 可点击切换。
5. resize、最小化和恢复不崩溃、不黑屏。
6. 后端名称出现在窗口标题或启动信息中，防止误测。

性能结果必须记录平台、后端、GPU、驱动和 TGFX commit。软件实现（例如 WARP、SwiftShader）只能用于功能测试，不能与硬件后端性能混用。

## 实施状态

已完成：

- 统一的 `BENCHMARK_BACKEND` 参数、平台矩阵校验和旧 TGFX 后端参数拒绝。
- Windows `OPENGL`、`ANGLE`、`VULKAN`、`D3D12` 窗口接入。
- macOS `OPENGL`、`METAL` 窗口接入。
- Web `WEBGL`、`WEBGPU` 窗口接入和 Emscripten 链接参数切换。
- 窗口标题显示实际后端；ANGLE DLL 部署到可执行文件目录。

本地 Windows 已完成四个后端的 Release 编译和窗口运行冒烟测试。macOS 和 Web 实际构建与运行仍需分别在安装 Xcode/Metal 和 Emscripten 4.0.15 的环境中验证。
