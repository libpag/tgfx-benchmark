# TGFX Benchmark Stage 9

Date: 2026-10-27.

## Scope

Stage 9 provides continuously animated MotionMark workloads through TGFX and
makes the TGFX source root pluggable.

## MotionMark Scenes

The former hand-authored TGFX scene set has been removed. The registered
MotionMark set follows the current Master Suite:

- `motionmark.multiply`
- `motionmark.canvas-arcs`
- `motionmark.leaves`
- `motionmark.canvas-lines`
- `motionmark.images`
- `motionmark.design`
- `motionmark.suits`

The scenes preserve the workload categories and visual behavior of upstream
MotionMark, but use TGFX APIs. They are not MotionMark score-compatible. The
Images adaptation exercises TGFX image transforms and sampling rather than
Canvas 2D `ImageData` mutation. See `docs/BENCHMARK_CASES.md` for source revision,
default counts, and API coverage.

## Animation Policy

- Motion uses elapsed wall-clock time, capped at 100 ms after a stall.
- Particle parameters are deterministic and do not use system randomness.
- Every scene remains continuously animated throughout preview and measurement.
- Ramp transitions are distributed over 200 ms.
- Load changes do not shrink unrelated objects or move the entire composition.

## Pluggable TGFX Source

The benchmark UI consumes four prebuilt Web packages:

```sh
cd web
npm run build:packages
```

`aot-webgl` and `aot-webgpu` use the AOT snapshot;
`jit-webgl` and `jit-webgpu` use TGFX main. The UI switches packages directly,
so AOT off is not used as a substitute for the main/JIT build. Native builds
still use:

```sh
cmake -S . -B build-bench-custom-tgfx -DTGFX_DIR=/path/to/tgfx
```

The selected root is used consistently by C++ CMake, the Emscripten build, and
the TypeScript binding. Each package emits `packages/<package-id>/tgfx-build.json`
containing the source root, full Git commit, source mode, dirty flag, codegen
mode, backend, artifact hashes, and AOT shader-bundle identity when applicable.
Official builds export the configured remote ref into a clean snapshot and do
not include worktree changes. Dirty optimization candidates use the separate
`--candidate` command, optionally accept a tag, and preserve the source patch
identity.

## Manual Live Run

Manual mode keeps the animated scene visible until `End & report`. Parent UI
polling uses only live-stats JSON. The report identifies the run as manual,
records `user-stop`, and includes retained frame samples. Runs longer than 8192
frames retain the latest 8192 samples and mark that policy explicitly.

## Verification

Acceptance is performed through:

```sh
node tools/benchmark-runner/test-adaptive.js
node tools/benchmark-runner/test-adaptive.js --negative-control

PLAYWRIGHT_MODULE=/path/to/playwright \
  BENCHMARK_PACKAGES=candidate-aot-webgl,candidate-aot-webgpu,jit-webgl,jit-webgpu \
  node tools/benchmark-runner/test-cold-start.js

PLAYWRIGHT_MODULE=/path/to/playwright \
  BENCHMARK_ENGINE_MODES=jit \
  node tools/benchmark-runner/smoke-ui.js

PLAYWRIGHT_MODULE=/path/to/playwright \
  BENCHMARK_AOT_PACKAGES=candidate-aot-webgl,candidate-aot-webgpu \
  node tools/benchmark-runner/audit-aot-coverage.js

ANIMATION_ASSERT=1 PLAYWRIGHT_MODULE=/path/to/playwright \
  node tools/benchmark-runner/audit-animation.js
```

The browser suite must exercise both WebGL and WebGPU, all registered scenes,
multi-frame visual changes, image-resource loading, manual/fixed/ramp modes, and
the narrow viewport. Candidate packages are intentionally absent from the UI
selector; use their direct package URLs for AOT runtime and coverage checks.
Measurement results from different revisions or backends must include their
scene revision and full build identity.
