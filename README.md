## Introduction
 tgfx-benchmark is benchmark project for tgfx.

## Platform Support
- MacOS 10.15 or later
## Build Prerequisites
tgfx-benchmark use C++17 features. Here are the minimum tools needed to build TGFX on different platforms:
- Xcode 11.0+
- NodeJS 14.14.0+

## Dependencies

tgfx-benchmark uses the [**depsync**](https://github.com/domchen/depsync) tool to manage third-party dependencies.

**For macOS platform：**

Run this script from the root of the project:

```
./sync_deps.sh
```

This script will automatically install the necessary tools and sync all third-party repositories.

## Getting Started
### macOS


In the `mac/` directory, run the following command or double-click it:

```
./gen_mac
```

This will generate a project for the native architecture, such as `arm64` for Apple Silicon Macs or
`x64` for Intel Macs. If you want to generate a project for a specific architecture, use the `-a`
option, for example:

```
./gen_mac -a x64
```

Finally, open Xcode and launch the `mac/BenchViewer.xcworkspace`. You are all set!

