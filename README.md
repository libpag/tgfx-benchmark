## Introduction
 A benchmark project for the [TGFX](https://github.com/Tencent/tgfx) graphics library. It is used to 
 test the performance of the TGFX graphics library on different platforms.

There is also a related project, [skia-benchmark](https://github.com/libpag/skia-benchmark), which
tests the performance of the [Skia](https://skia.org/) graphics library using the same benchmark cases.

You can also run the benchmark online at [tgfx.org](https://tgfx.org/benchmark) to compare the 
performance of TGFX and Skia on the web platform.


## Getting Started

Before building the projects, please carefully follow the instructions in the
[**Build Prerequisites**](https://github.com/Tencent/tgfx?tab=readme-ov-file#build-prerequisites) 
and [**Dependencies**](https://github.com/Tencent/tgfx?tab=readme-ov-file#dependencies) sections.
These will guide you through the necessary steps to set up your development environment.

The GPU backend is selected at CMake configure time with `BENCHMARK_BACKEND`. Use a separate build
directory for each backend. If the option is omitted or set to `AUTO`, the existing defaults are used:
OpenGL on Windows and macOS, and WebGL on Web.

| Platform | Supported backends |
| --- | --- |
| Windows | `OPENGL`, `ANGLE`, `VULKAN`, `D3D12` |
| macOS | `OPENGL`, `METAL` |
| Web | `WEBGL`, `WEBGPU` |

Only `BENCHMARK_BACKEND` is accepted as the backend selection entry. Do not pass TGFX internal
backend options such as `TGFX_USE_ANGLE` or `TGFX_USE_VULKAN`.

### macOS

macOS supports the `OPENGL` and `METAL` backends. OpenGL is the default. In CLion, add one of the
following CMake options to the profile, then build and run the `Benchmark` target using the Release
configuration:

```
-DBENCHMARK_BACKEND=OPENGL
-DBENCHMARK_BACKEND=METAL
```

If you prefer Xcode, generate the project from the repository root. For OpenGL, run:

```
./gen_mac -DBENCHMARK_BACKEND=OPENGL
```

For Metal, run:

```
./gen_mac -DBENCHMARK_BACKEND=METAL
```

The script generates a project for the native architecture, such as `arm64` for Apple Silicon Macs or
`x64` for Intel Macs. To select a specific architecture, add the `-a` option:

```
./gen_mac -a x64 -DBENCHMARK_BACKEND=METAL
```

The generated project is written to `mac/TGFX-Benchmark.xcodeproj`. Regenerate it when switching
backends, then open it in Xcode and launch the `Benchmark` target. The application title displays the
selected backend.

### Windows

Windows supports the `OPENGL`, `ANGLE`, `VULKAN`, and `D3D12` backends. OpenGL is the default.

To use CLion, open the repository root and go to `File->Settings` >
`Build, Execution, Deployment->ToolChains`. Select the Visual Studio toolchain with either `amd64`
(recommended) or `x86`. In `Build, Execution, Deployment->CMake`, select the Ninja generator and add
one backend option to the CMake profile, for example:

```
-DBENCHMARK_BACKEND=D3D12
```

Use a separate CLion profile and build directory for each backend, then build and run the `Benchmark`
target using the Release configuration.

To use Visual Studio, open the `x64 Native Tools Command Prompt for VS 2019` and configure the desired
backend in its own directory:

```
cmake -S . -B ./win/Release-x64-opengl -G "Visual Studio 16 2019" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release" -DBENCHMARK_BACKEND=OPENGL
cmake -S . -B ./win/Release-x64-angle -G "Visual Studio 16 2019" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release" -DBENCHMARK_BACKEND=ANGLE
cmake -S . -B ./win/Release-x64-vulkan -G "Visual Studio 16 2019" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release" -DBENCHMARK_BACKEND=VULKAN
cmake -S . -B ./win/Release-x64-d3d12 -G "Visual Studio 16 2019" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release" -DBENCHMARK_BACKEND=D3D12
```

Build a configured backend from the command line with:

```
cmake --build ./win/Release-x64-d3d12 --config Release --target Benchmark
```

To generate an `x86` Debug project, open the `x86 Native Tools Command Prompt for VS 2019` and use a
separate output directory:

```
cmake -S . -B ./win/Debug-x86-opengl -G "Visual Studio 16 2019" -A Win32 -DCMAKE_CONFIGURATION_TYPES="Debug" -DBENCHMARK_BACKEND=OPENGL
```

Open the `Benchmark.sln` file from the selected output directory and set `Benchmark` as the startup
project. ANGLE builds automatically copy `libEGL.dll` and `libGLESv2.dll` next to `Benchmark.exe`.
Vulkan requires a Vulkan-capable driver and loader. The application title displays the selected
backend.

### Web

To get started, go to the `web/` directory and run the following command to install the necessary
node modules:

```
npm install
```

Then, in the `web/` directory, run the following command to build the demo project:

```
npm run build
```

This builds the default WebGL multithreaded version and generates `benchmark.js` and
`benchmark.wasm` in the `web/demo/wasm-mt` directory. Next, you can start an HTTP server by running
the following command:

```
npm run server
```

This will open [http://localhost:8061/index.html](http://localhost:8061/index.html) in your default
browser. You can also open it manually to view the demo.

To build and run the WebGPU multithreaded version, use:

```
npm run build:webgpu
npm run server:webgpu
```

The build script installs and activates Emscripten `4.0.15`. The WebGPU page requires a browser
with WebGPU support and must be opened from `localhost` or HTTPS.

To debug the C++ code, install the browser plugin:
[**C/C++ DevTools Support (DWARF)**](https://chromewebstore.google.com/detail/cc++-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb).
Then, open Chrome DevTools, go to Settings > Experiments, and enable the option
**WebAssembly Debugging: Enable DWARF support**.

Next, replace the previous build command with:

```
npm run build:debug
```

With these steps completed, you can debug C++ files directly in Chrome DevTools.

The above commands build and run a multithreaded version.

>**⚠️** In the multithreaded version, if you modify the filename of the compiled output benchmark.js, you need to search for
> the keyword "benchmark.js" within the benchmark.js file and replace all occurrences of "benchmark.js" with the new filename.
> Failure to do this will result in the program failing to run. Here's an example of how to modify it:

Before modification:

```js
    // filename: benchmark.js
    var worker = new Worker(new URL("benchmark.js", import.meta.url), {
     type: "module",
     name: "em-pthread"
    });
```

After modification:

```js
    // filename: benchmark-test.js
    var worker = new Worker(new URL("benchmark-test.js", import.meta.url), {
     type: "module",
     name: "em-pthread"
    });
```

To build a single-threaded version, add the suffix `:st` to the commands:

```
npm run build:st
npm run build:st:debug
npm run server:st
npm run build:webgpu:st
npm run build:webgpu:st:debug
npm run server:webgpu:st
``` 

To build the demo project in CLion, open the `Settings` panel and go to `Build, Execution, Deployment` > `CMake`.
Create a new build target and set the `CMake options` to:

```
DCMAKE_TOOLCHAIN_FILE="path/to/emscripten/emscripten/version/cmake/Modules/Platform/Emscripten.cmake"
```

After creating the build target, adjust the `Configurations` to match the new build target. This will
allow you to build the tgfx library in CLion.

Additionally, when using `ESModule` for your project, you need to manually include the generated
`.wasm` file in the final web program. Common packing tools often ignore the `.wasm` file. Also,
make sure to upload the `.wasm` file to a server so users can access it.