## Introduction
 A benchmark project for the TGFX graphics library. It is used to test the performance of the TGFX
 graphics library on different platforms.

## Getting Started

Before building the projects, please carefully follow the instructions in the
[**Build Prerequisites**](https://github.com/Tencent/tgfx?tab=readme-ov-file#build-prerequisites) 
and [**Dependencies**](https://github.com/Tencent/tgfx?tab=readme-ov-file#dependencies) sections.
These will guide you through the necessary steps to set up your development environment.

### macOS
To get started, open the root directory in CLion. Then, go to `File->Settings` and navigate to 
`Build, Execution, Deployment->CMake`. Add a new profile with the Release build type. Now you are 
ready to build and run the Benchmark target.

If you prefer using XCode IDE, go to the `mac/` directory, run the following command or double-click
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

