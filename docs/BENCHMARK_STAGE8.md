# TGFX Benchmark Stage 8

Date: 2026-10-26.

## Scope

Stage 8 adds the first native reference backend using Metal on macOS.

## Implementation

- Native benchmark state recorder
- Stable scene ID lookup
- Scene count configuration
- Per-frame callback, scene draw, flush, and submit timing
- JSON summary output
- Metal backend build

## Build

```sh
cmake -S . -B build-bench-metal-13 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBENCHMARK_BACKEND=METAL \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build build-bench-metal-13 --target Benchmark -j 8
```

The explicit deployment target avoids the libc++ compatibility warning in newer Xcode toolchains.

## Run

```sh
node tools/benchmark-runner/run-native-metal.cjs \
  --case motionmark.multiply \
  --count 600 \
  --warmup 500 \
  --measurement 500 \
  --output results/stage8-metal-smoke.json
```

## Historical Verification

- Metal build: passed
- Native smoke: passed
- Scene: the original verification used a now-retired scene and is not comparable
  with current results.
- Count: 600
- Warmup: 500 ms
- Measurement: 500 ms
- Samples: 114
- Overflow: false
- Execution status: completed
