# Backend Optimization Tooling

This interface is for backend optimization agents and scripts. It does not add
controls, metrics, or package selectors to the benchmark UI.

## Candidate Package

An agent can package a modified TGFX checkout without touching an official
package:

```sh
cd tgfx-benchmark/web
node script/build-package.js \
  --package=aot-webgpu \
  --tgfx-root=/path/to/modified/tgfx \
  --candidate
```

The output is `packages/candidate-aot-webgpu/`. To retain multiple experiments,
add a tag:

```sh
node script/build-package.js \
  --package=aot-webgpu \
  --tgfx-root=/path/to/modified/tgfx \
  --candidate \
  --tag=batch-01
```

This writes `packages/candidate-batch-01-aot-webgpu/`. Without a tag, the
default candidate slot is replaced atomically.

Its `tgfx-build.json` records:

- `packageRole: "candidate"`;
- `basePackageId` and `baseCommit`;
- the configured remote ref, remote commit, and whether `HEAD` matches it;
- dirty state and source patch SHA-256;
- `candidate-source.patch`, generated from tracked and untracked non-ignored
  files;
- compatibility patch IDs and hashes;
- the benchmark source patch;
- the usual Wasm, glue, entry, and AOT shader-bundle hashes.

Official builds use a clean Git `HEAD` snapshot, ignore unrelated dirty
experiments, and apply the compatibility patches in
`web/patches/compatibility/`. Candidate packages are discovered from
`packages/index.json` or `/api/packages` and appear in the UI package selector.

Builds are serialized and published atomically. A failed build cannot replace a
previously working package.

## WebGPU Counters

The counter runner returns one JSON object and can be called as a CLI or as a
module:

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

`run()` does not print unless `main()` is used by the CLI. The returned report
contains per-command-buffer totals, p50, p95, and max for:

- render/compute passes, draws, instances, vertices, and indices;
- pipeline, bind-group, vertex-buffer, and index-buffer binds;
- uploads, upload bytes, copies, copied bytes, and queue submissions;
- buffer, texture, render/compute-pipeline, bind-group, and shader-module creation;
- canvas texture acquisition and logical resource bytes.

`drawCalls` includes both `draw` and `drawIndexed`; `indexedDrawCalls` is a
subset. `uniquePipelines` is unique within each command buffer. Uploaded CPU
bytes and texture payload bytes are reported separately. Resource memory is an
upper bound based on explicit `destroy()` calls because garbage collection is
not observable.

These counters are diagnostic and can perturb frame timing. Use the existing
timing reports for performance acceptance and use counters to explain changes
in passes, bindings, uploads, pipeline compilation, and resource creation.

## First-frame CPU metrics

A cold-start summary exposes one canonical first-frame object:

```json
{
  "firstFrame": {
    "available": true,
    "reason": null,
    "callbackUs": 41000,
    "sceneDrawUs": 31000,
    "flushUs": 2100,
    "submitUs": 321
  }
}
```

All durations use microseconds. `submitUs` may legitimately be zero.
Unavailable metrics use `available: false` and `null`, never zero as a
substitute for "not measured". The detailed phase metrics remain in exported
JSON and programmatic summaries but are intentionally omitted from the compact
benchmark UI.

## Verification

```sh
node tools/benchmark-runner/test-candidate-package.js
node tools/benchmark-runner/test-webgpu-counter-probe.js
git diff --check
```
