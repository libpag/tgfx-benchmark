## Introduction
 A benchmark project for the TGFX graphics library. It is used to test the performance of the TGFX
 graphics library on different platforms.

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

Finally, open Xcode and launch the `mac/Benchmark/Benchmark.xcworkspace`. You are all set!

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
cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_CONFIGURATION_TYPES="Debug" -B ./win/Debug-x64
```

This will generate a project for the `x64` architecture with the `Debug` configuration. To generate
a project for the `x86` architecture with the `Release` configuration, open the
`x86 Native Tools Command Prompt for VS 2019` and run the following command:

```
cmake -G "Visual Studio 16 2019" -A Win32 -DCMAKE_CONFIGURATION_TYPES="Release" -B ./win/Release-x86
```

Finally, open the `Benchmark.sln` file in the `win/Debug-x64/` or `win/Release-x86/` directory, and set
the `Benchmark` project as the startup project. You are all set!