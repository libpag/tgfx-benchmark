# Benchmark Suite Scope

Date: 2026-10-30.

This document is the authoritative scope for the Web benchmark. Historical
stage documents describe how the platform was built; they do not extend the
supported suite.

## Official Suite

The official Web benchmark contains:

- seven steady-state MotionMark workloads;
- one cold-start Shader Mix workload;
- five original TGFX particle workloads in their own UI group;
- two backends, WebGL and WebGPU;
- two code paths, AOT and JIT.

Official steady-state cases:

The seven workloads use scene revision `v2`: the same count units and drawing
categories with revised composition and continuous, viewport-bounded motion.
The five original particle workloads are unchanged. Compare performance within
the same scene revision; visual changes can change overdraw and cache behavior.

- `motionmark.multiply`
- `motionmark.canvas-arcs`
- `motionmark.leaves`
- `motionmark.canvas-lines`
- `motionmark.images`
- `motionmark.design`
- `motionmark.suits`

Official cold-start case:

- `coldstart.shader-mix`, scene revision `v7`

Scene `v7` has 13 deterministic, non-YUV motifs. I420 and NV12 are not part of
the official suite because AOT WebGL I420 has caused an IOGPU command queue hang
and WindowServer watchdog failure. Do not add a known crash path back to the UI,
automatic acceptance, AOT coverage, or performance scoring.

## Original TGFX Workloads

The five `legacy.particles-*` stable IDs are available in the benchmark UI and
remain resolvable by direct URL. Their antialias and fill/stroke options are
available only while one of these scenes is selected.

The original workloads stay in a separate UI group and preserve their historical
baseline. Do not combine their counts or scores with MotionMark workloads, and
select the same antialias/stroke setting for paired runs. They are excluded from
the MotionMark acceptance matrix unless a separate AOT coverage decision is
made.

`ParticleBench` remains the renderer for these scenes and the separate
interactive Web demo.

YUV coverage is a separate renderer diagnostic:

- it must not run as part of Web benchmark acceptance;
- it must not contribute to a benchmark score;
- it requires an isolated Apple Silicon machine when hardware execution is
  needed;
- CPU-only hit/miss results do not prove hardware safety.

## Change Policy

The benchmark suite grows only when a new case replaces an existing case or has
an explicit scope decision. Do not add ad-hoc scenes, shader motifs, package
combinations, or diagnostic modes to the official suite while investigating a
single renderer issue.

Every official case must have:

- a stable ID and scene revision;
- a deterministic count unit;
- a bounded default load;
- a defined test intent;
- an animation/correctness check;
- AOT evidence on both official backends;
- a documented reason to remain in the suite.

Changes to this scope require updating this document, the case registry, the UI,
and the relevant automated acceptance in the same change.

Run the no-browser scope guard after changing the suite:

```sh
node tools/benchmark-runner/test-benchmark-scope.js
```
