# TGFX Web Benchmark Usage

Date: 2026-10-29.

The official scene and backend scope is fixed in `BENCHMARK_SCOPE.md`.

## 1. Install

```sh
cd web
npm install
```

## 2. Build

The browser benchmark uses four release, single-thread TGFX packages:

| Package | Source snapshot | TGFX ref | Backend |
| --- | --- | --- | --- |
| `aot-webgl` | `sources/tgfx-aot` | `feature/shenxianfei_aot_simplify` | WebGL |
| `aot-webgpu` | `sources/tgfx-aot` | `feature/shenxianfei_aot_simplify` | WebGPU |
| `jit-webgl` | `tgfx-main-perf` | `main` | WebGL |
| `jit-webgpu` | `tgfx-main-perf` | `main` | WebGPU |

Build all four packages:

```sh
cd web
npm run build:packages
```

Or rebuild one package:

```sh
npm run build:package:aot-webgl
npm run build:package:aot-webgpu
npm run build:package:jit-webgl
npm run build:package:jit-webgpu
```

Each package is written to `../packages/<package-id>/` and contains its own
`index.html`, `tgfx-build.json`, benchmark JavaScript, Wasm, and copied static
assets. `tgfx-build.json` is authoritative for the TGFX commit, backend,
codegen mode, dirty state, build patches, and artifact hashes. AOT packages also
record the embedded shader bundle identity and toolchain ABI.

See `PACKAGE_WORKFLOW.md` for the source-layer, clean-snapshot,
compatibility-patch, tagged-candidate, and package-catalog contract.

Builds fail fast if the configured remote ref is unavailable. Official builds
export that ref into a temporary clean snapshot, so a stale worktree `HEAD` and
unrelated dirty experiments are not included. Refresh the snapshots before
rebuilding:

```sh
git -C ../../tgfx-aot fetch origin --prune
git -C ../../tgfx-main-perf fetch origin --prune
```

Then update each snapshot to the required remote commit before running the
package build. Compatibility patches under `web/patches/compatibility/` are
applied to the clean snapshot and recorded in the manifest.

For backend optimization agents, a dirty source can be packaged without
overwriting an official package:

```sh
cd web
node script/build-package.js \
  --package=aot-webgpu \
  --tgfx-root=/path/to/tgfx-candidate \
  --candidate
```

This writes `packages/candidate-aot-webgpu/`, records `packageRole:
"candidate"`, the base package and commit, remote-ref position, dirty state,
and `candidate-source.patch` with its SHA-256 in `tgfx-build.json`. The patch
includes tracked and untracked non-ignored files.

Use `--tag=<name>` to retain a uniquely named candidate for A/B comparison:

```sh
node script/build-package.js \
  --package=aot-webgpu \
  --tgfx-root=/path/to/tgfx-candidate \
  --candidate \
  --tag=batch-01
```

This writes `packages/candidate-batch-01-aot-webgpu/`. Candidate packages are
published atomically, recorded in `packages/index.json`, and listed by the
benchmark UI package selector. Omitting `--candidate` builds an official clean
`HEAD` snapshot with the curated compatibility patches.

### Select a pluggable TGFX source

The four benchmark commands use the snapshots above. For a separate diagnostic
package, the underlying build commands still accept `TGFX_ROOT` or
`--tgfx-root`:

```sh
TGFX_ROOT=/path/to/tgfx npm run build:st
npm run build:webgpu:st -- --tgfx-root=/path/to/tgfx
```

The same TGFX root is used by C++ CMake, the Emscripten build, and the TypeScript
binding build. Native builds use CMake directly:

```sh
cmake -S . -B build-bench-custom-tgfx \
  -DTGFX_DIR=/path/to/tgfx \
  -DBENCHMARK_BACKEND=WEBGPU
```

## 3. Start the local server

From the repository root:

```sh
cd web
NO_OPEN=1 node server.js
```

Default URL:

```text
http://localhost:8061/benchmark-ui.html
```

The UI defaults to `AOT + WebGL` and switches the iframe among the four built
packages. It displays the selected package's compact TGFX commit, with the full
commit in the tooltip, and its codegen mode,
and includes both values in exported reports. Switching to JIT loads the
main/runtime package; it does not disable AOT inside an AOT package.

AOT reports also retain runtime hit/miss and fallback-reason counters. If the AOT
route drops a draw because of a lookup or vertex-interface failure, the report is
marked `AOT 不完整` and must not be used as a valid AOT comparison. The intentional
`DeferredTexture` fallback is excluded from that validity decision.

Direct package pages:

```text
http://localhost:8061/packages/aot-webgl/index.html
http://localhost:8061/packages/aot-webgpu/index.html
http://localhost:8061/packages/jit-webgl/index.html
http://localhost:8061/packages/jit-webgpu/index.html
```

Programmatic WebGPU render counters are emitted as JSON without adding UI
fields. See `docs/BACKEND_OPTIMIZATION_TOOLING.md` for the module contract and
counter limitations, or use the CLI:

```sh
PLAYWRIGHT_MODULE=/path/to/playwright \
node tools/benchmark-runner/webgpu-counter-runner.js \
  --package=aot-webgpu \
  --case=motionmark.canvas-lines \
  --count=16 \
  --test-intent=steady-state \
  --measurement=3000 \
  --output=results/aot-webgpu-counters.json
```

The runner reports command-buffer records and per-record totals/distributions
for render/compute passes, draws, pipeline and bind-group binds, uploads,
copies, submissions, pipeline/module creation, and logical live-resource
bytes. `uploadedBytes` is the CPU source-byte count; `uploadedPayloadBytes`
excludes texture row padding. Live resource bytes only decrease on explicit
buffer/texture destruction, so they are an upper bound rather than a heap
measurement. These counters are diagnostic and can perturb timing; use the
existing timing reports for performance acceptance.

Backend programs can also require the runner directly; `run()` returns the
same report object that is written to stdout:

```js
const {run} = require('./tools/benchmark-runner/webgpu-counter-runner');
const report = await run({
  baseUrl: 'http://127.0.0.1:8061',
  packageId: 'candidate-aot-webgpu',
  caseId: 'motionmark.canvas-lines',
  count: 16,
  testIntent: 'steady-state',
  initialWindowFrames: 30,
  measurementMs: 3000,
  output: null,
});
```

The UI opens on the Multiply scene in Ramp mode with the official AOT WebGL
package when available. The package selector lists official and tagged
candidate builds discovered from their manifests, and the build identity shows
which artifact is running. Its
canvas fills the scene area and renders at the actual viewport size multiplied by DPR.
The displayed and exported resolution is the real backing-store pixel size.
Resize the window before starting a measurement: a size change during a run
invalidates that run and stops it. Direct benchmark URLs with `width` and
`height` keep their existing fixed-size semantics.

The interface uses an explicit test intent:

- `Cold-start`: render `coldstart.shader-mix` at a fixed initial count, record
  first command submission, the next browser refresh callback after the first
  frame, the first frame, and 30 render callbacks after that callback.
- `Steady-state`: discard the first 10 successful draw/flush frames and then
  use one of the existing run modes.

Selecting Shader Mix also selects cold-start intent. Its animated preview uses
at most 32 units in a separate instance, with the actual preview count shown in
the scene header and HUD. Starting a test replaces that instance and collects the
configured initial load plus 30 frames after visibility, then opens the report.
The cold-start report also lists the render-path groups exercised by the scene.
Initialization failures display their original error in the interface. Official
AOT packages must be rebuilt after the benchmark scene registry changes.

The steady-state interface has three run modes:

- `Manual`: discard startup frames, then continuously render and measure until
  `End & report`.
  The scene stays animated throughout the measurement.
- `Fixed window`: discard startup frames, measure for the configured window,
  then open a report.
- `Ramp`: start from the configured object count and grow toward the target
  without an object-count maximum. Growth transitions are distributed over
  200 ms, and each candidate is evaluated with 30 non-overlapping frame
  intervals at the same actual object count after a 100 ms transition exclusion.
  The next count is scaled by half of the measured effective frame-cost
  headroom, capped at 45%, so it moves quickly far from the target and more
  cautiously as it approaches. Two consecutive failed windows freeze the count
  as an estimated threshold. The scene stays animated, and the user explicitly
  ends the run to open the report.
  Severe overload can fail early using at least eight intervals over at least
  one second when average cadence is below half the target. Runtime errors and
  ten seconds without draw progress invalidate the run.

The legacy ParticleBench controller added a configured step on every frame and
fropped the count on the first low-FPS/high-draw-time observation. The current
quick ramp uses the same broad idea, but confirms the threshold with two
same-count windows and derives the step size from measured frame cost. Its
report is an estimated capacity, not a long-duration steady-state guarantee.

The UI shows the current loaded `requestAnimationFrame` cadence as
`browser refresh ceiling … times/second`; this is a callback cadence, not a
hardware refresh-rate or GPU execution time measurement. An impossible target
is rejected only when there are at least 16 unloaded intervals from
initialization. A slow preview must not be mistaken for the display ceiling. A window passes when FPS
is at least `target - max(0.5 FPS, target * 1%)` and p95 is at most
`1000/target + max(1 ms, (1000/target) * 5%)`. Two consecutive failed windows
define the estimated threshold. The report analyzes the final same-count
observation window, including when the raw frame ring has wrapped to its latest
8192 samples.

The scene list includes seven workloads from the current MotionMark Master
Suite, the dedicated `coldstart.shader-mix` stress scene, and the original five
TGFX particle scenes in their own group. Antialias and fill/stroke controls are
shown only for the original particle group and are recorded in the run report.
Former hand-authored TGFX scenes are no longer registered. TGFX and MotionMark scenes lay out their workload against the current canvas
size, so increasing the object count creates additional visible draw work
rather than placing objects outside the viewport. Fixed grid anchors are reused
as visible overlapping layers with different within-cell positions, so growth does not shrink every object or
teleport the whole layout. All scenes now animate continuously at a wall-clock
rate; particle first emission uses staggered trajectory phases to avoid burst
rings when a count step is applied. Images are decoded before benchmark timing and passed
to TGFX as a native Web image, not as a URL interpreted as a Wasm file path.
Scenes report their actual
accepted count back to the controller; if a scene cannot grow further, the
report marks it as capacity-limited instead of recording phantom objects.
The scene HUD shows rolling FPS, callback CPU time, current object count and
measurement phase. The sidebar shows the attempted peak, settled count and
target frame budget. The report includes the initial/peak/settled counts,
time to convergence, retraction count, the retained object-count trajectory,
and average FPS plus p50/p95/p99 frame intervals from the stable interval only.
The JSON also retains raw frame samples, the window policy and stability-loss
count. Failed/unsettled runs never fall back to presenting ramp samples as
steady-state statistics. Capacity-limited
and unreachable-target runs are reported as failures rather than stable results.
Ramp reports separate the initial, peak, final actual, and settled counts. An
unsettled report uses the final actual count in its title and session history;
it never substitutes the initial count when no settled count exists.
Frame intervals are callback-start to callback-start, not GPU execution time.
WebGL and WebGPU require separate runs in the same environment for comparisons.

The history table keeps AOT and JIT runs separate but makes the code-path
difference explicit. Every row has an execution-path column. Cold-start history
shows first command submission, the next browser refresh callback after the
first frame, first-frame phase timings and the p95 of the following 30 frame
intervals. Steady-state history also shows first submit, startup isolation,
CPU callback p95 and AOT hit/miss evidence. JIT rows report `JIT` instead of
pretending an AOT cache was used. Invalid AOT runs remain visible with an
`AOT 不完整` status and are not valid comparison evidence.

Long manual observations retain the **most recent 8192 frames** if the recorder
fills up. The report identifies this condition and shows the number of frames
recorded and retained. A report can be downloaded as JSON and loaded back in
the UI. Earlier reports without newer fields still display available metrics.

Browser smoke test, with the local server running:

```sh
PLAYWRIGHT_MODULE=/path/to/playwright \
  BENCHMARK_ENGINE_MODES=jit \
  node tools/benchmark-runner/smoke-ui.js
```

The original TGFX particle group has a focused UI regression:

```sh
PLAYWRIGHT_MODULE=/path/to/playwright \
  node tools/benchmark-runner/test-legacy-ui.js
```

On macOS, launch Playwright outside a restricted GUI sandbox if browser
initialization aborts in HIServices. The smoke test checks both backends,
animated scenes, manual/fixed/ramp operation and a narrow viewport. It supports
both codegen modes; the command above uses the current official JIT packages.
Set `BENCHMARK_ENGINE_MODES=aot` or `jit` to select the UI matrix. Candidate
packages are intentionally absent from the UI selector; validate them through
direct package URLs and the cold-start runner.

Four-package cold-start acceptance:

```sh
PLAYWRIGHT_MODULE=/path/to/playwright \
  BENCHMARK_PACKAGES=candidate-aot-webgl,candidate-aot-webgpu,jit-webgl,jit-webgpu \
  node tools/benchmark-runner/test-cold-start.js
```

Each package runs in a fresh browser process, reports first command submission
and the next refresh callback after the first frame, collects 30 following
render callbacks, and verifies a fresh 10-frame steady-state startup isolation.
AOT packages must load their bundle with no actionable miss.

Focused four-package switching and report identity check:

```sh
PLAYWRIGHT_MODULE=/path/to/playwright \
  node tools/benchmark-runner/smoke-packages.js
```

This checks the selected package URL, compact TGFX commit plus full tooltip in the UI, runtime AOT
evidence, a short fixed run, the exported report identity and the narrow layout.

AOT scene-coverage audit:

```sh
PLAYWRIGHT_MODULE=/path/to/playwright \
  BENCHMARK_AOT_PACKAGES=candidate-aot-webgl,candidate-aot-webgpu \
  node tools/benchmark-runner/audit-aot-coverage.js
```

It opens every official scene through both AOT packages and fails if any
non-deferred `VertexLayoutMismatch`, lookup, or pipeline fallback is recorded.
The detailed result is written to `results/aot-package-coverage.json`.

Scope guard (no browser):

```sh
node tools/benchmark-runner/test-benchmark-scope.js
```

All offline benchmark checks:

```sh
cd web
npm run test:offline
```

Deterministic controller and report tests (no browser timing dependencies):

```sh
node tools/benchmark-runner/test-adaptive.js
node tools/benchmark-runner/test-adaptive.js --negative-control
```

All-scene animation and actual image-resource acceptance:

```sh
ANIMATION_ASSERT=1 NODE_PATH=/path/to/playwright/node_modules \
  node tools/benchmark-runner/audit-animation.js
```

One direct query or runner invocation produces one measurement. Repetition and
cache isolation are controlled by launching another invocation.

## 4. Run a case directly

WebGL:

```text
http://localhost:8061/packages/aot-webgl/index.html?benchmark=1&testIntent=steady-state&case=motionmark.multiply&count=512&measurementMs=1000&width=900&height=600&aot=1
```

WebGPU:

```text
http://localhost:8061/packages/aot-webgpu/index.html?benchmark=1&testIntent=steady-state&case=motionmark.multiply&count=512&measurementMs=1000&width=900&height=600&aot=1
```

Adaptive ramp:

```text
http://localhost:8061/packages/jit-webgl/index.html?benchmark=1&testIntent=steady-state&case=motionmark.multiply&count=512&adaptive=1&targetFps=60&stableMs=3000&width=900&height=600&aot=0
```

Cold-start:

```text
http://localhost:8061/packages/aot-webgpu/index.html?benchmark=1&testIntent=cold-start&case=coldstart.shader-mix&count=256&initialWindowFrames=30&width=900&height=600&aot=1
```

## 5. Run a fixed pilot

```sh
NODE_PATH=/path/to/playwright/node_modules \
node tools/benchmark-runner/run-web-case.js \
  --engine aot \
  --backend webgl \
  --case motionmark.multiply \
  --count 512 \
  --test-intent steady-state \
  --recorder 1 \
  --measurement 1000 \
  --output results/stage7-fixed.json
```

Use `--runtime-probe=identity` for timing comparisons. The default `full` probe instruments
`writeBuffer`, draw and bind calls for diagnostics and must not be used for performance decisions.

## 6. Run a cold-start case

```sh
NODE_PATH=/path/to/playwright/node_modules \
node tools/benchmark-runner/run-web-case.js \
  --package aot-webgpu \
  --case coldstart.shader-mix \
  --count 256 \
  --test-intent cold-start \
  --initial-window 30 \
  --recorder 1 \
  --output results/cold-start-webgpu.json
```

## 7. Preflight artifacts

With the local server running:

```sh
BENCHMARK_PACKAGES=candidate-aot-webgl,candidate-aot-webgpu,jit-webgl,jit-webgpu \
  node tools/benchmark-preflight/validate-web-backends.js http://localhost:8061
```

The preflight verifies the selected package manifests, served/disk artifact
hashes, TGFX commit freshness against `origin/main` or the AOT remote ref, and
that only AOT packages contain shader-bundle identities. Official packages must
come from clean source; candidate packages must preserve a matching
`candidate-source.patch` with its recorded size and SHA-256.

For a frozen comparison that intentionally uses the same historical source revision, set
`BENCHMARK_ALLOW_STALE=1`. The report then uses `validityStatus: "valid-with-stale"` and records
each package's actual and latest remote commit; do not use this flag for current-branch acceptance.

## 8. Generate a report

```sh
node tools/benchmark-report/generate-report.js \
  --input tools/benchmark-report/fixtures/stage1-summary.json \
  --output /tmp/tgfx-report.md
```
