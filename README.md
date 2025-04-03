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

### macOS
To get started, open the root directory in CLion. Then, build and run the `Benchmark` target using the
Release configuration.

If you prefer using XCode IDE, go to the root directory, run the following command or double-click
it:

```
./gen_mac
```

This will generate a project for the native architecture, such as `arm64` for Apple Silicon Macs or
`x64` for Intel Macs. If you want to generate a project for a specific architecture, use the `-a`
option, for example:

```
./gen_mac -a x64
```

Finally, open Xcode and launch the `mac/TGFX-Benchmark.xcodeproj`. You are all set!

### Windows

To get started, open the root directory in CLion. Then, go to `File->Settings` and navigate to
`Build, Execution, Deployment->ToolChains`. Set the toolchain to `Visual Studio` with either `amd64`
(recommended) or `x86` architecture. It's also recommended to use the `Ninja` generator for CMake to
speed up the build process. You can set this in `Build, Execution, Deployment->CMake` by choosing
`Ninja` in the `Generator` row. Once done, build and run the `Benchmark` target using the Release
configuration.

If you prefer using Visual Studio IDE, open the `x64 Native Tools Command Prompt for VS 2019` and
run the following command in the root directory:

```
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_CONFIGURATION_TYPES="Release" -B ./win/Release-x64
```

This will generate a project for the `x64` architecture with the `Release` configuration. To generate
a project for the `x86` architecture with the `Debug` configuration, open the
`x86 Native Tools Command Prompt for VS 2019` and run the following command:

```
cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_CONFIGURATION_TYPES="Debug" -B ./win/Debug-x86
```

Finally, open the `Benchmark.sln` file in the `win/Release-x64/` or `win/Debug-x86/` directory, and 
set the `Benchmark` project as the startup project. You are all set!

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

This will generate the `benchmark.js` and `benchmark.wasm` files in the `web/demo/wasm` directory.
Next, you can start an HTTP server by running the following command:

```
npm run server
```

This will open [http://localhost:8061/web/demo/index.html](http://localhost:8061/web/demo/index.html)
in your default browser. You can also open it manually to view the demo.

To debug the C++ code, install the browser plugin:
[**C/C++ DevTools Support (DWARF)**](https://chromewebstore.google.com/detail/cc++-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb).
Then, open Chrome DevTools, go to Settings > Experiments, and enable the option
**WebAssembly Debugging: Enable DWARF support**.

Next, replace the previous build command with:

```
npm run build:debug
```

With these steps completed, you can debug C++ files directly in Chrome DevTools.

The above commands build and run a multithreaded version. To build a single-threaded version,
just add the suffix ":st" to each command. For example:

```
npm run build:st
npm run build:st:debug
npm run serser:st
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