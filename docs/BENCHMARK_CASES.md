# TGFX Benchmark Case Registry

Date: 2026-10-30.

The authoritative suite scope is defined in `BENCHMARK_SCOPE.md`. The benchmark
UI exposes seven MotionMark steady-state workloads, one Shader Mix cold-start
workload, and five original TGFX particle workloads in a separate group.

## Original TGFX Particle Suite

These cases preserve the original TGFX benchmark scenes. Antialias and
fill/stroke settings apply only to this group and must stay identical in paired
comparisons.

| Case ID | Main APIs | Default count |
| --- | --- | ---: |
| `legacy.particles-rect` | `drawRect`, antialias/stroke options | 600 |
| `legacy.particles-circle` | `drawCircle` | 600 |
| `legacy.particles-oval` | `drawOval` | 600 |
| `legacy.particles-rrect` | `drawRoundRect` | 600 |
| `legacy.particles-star` | `drawPath` | 300 |

## MotionMark Master Suite

Source: `https://github.com/WebKit/MotionMark`, reviewed at commit
`2faa1512e8696cf113548534de01a7ce4c4700ab` (2026-10-04).

| Case ID | MotionMark test | TGFX workload | Default count |
| --- | --- | --- | ---: |
| `motionmark.multiply` | Multiply | rotating tile matrix with outward color waves and local drift | 512 |
| `motionmark.canvas-arcs` | Canvas Arcs | rotating arcs and sectors with gently oscillating sweep angles | 8 |
| `motionmark.leaves` | Leaves | transparent image particles, wind-like drift, rotation and smooth fade | 128 |
| `motionmark.canvas-lines` | Canvas Lines | four staggered circles with rotating, smoothly pulsing line sprays | 16 |
| `motionmark.images` | Images | distributed image mosaic with local orbits and tilting MotionMark assets | 64 |
| `motionmark.design` | Design | responsive multilingual text layers with small offsets and color shimmer | 8 |
| `motionmark.suits` | Suits | drifting heart, club, spade and diamond particles with edge reflection | 24 |

The seven TGFX ports use scene revision `v2`. Object sizes and counts remain
independent of the adaptive controller, and growth does not shrink objects or
remove their draw calls. Revisions preserve each workload's drawing categories,
but different placement and motion can change overdraw and performance slightly.
Use the same scene revision for paired AOT/JIT and WebGL/WebGPU comparisons.

## Cold-start Suite

| Case ID | Purpose | Shader groups | Default count |
| --- | --- | --- | ---: |
| `coldstart.shader-mix` | TGFX first-frame and first-visible latency | AA/complex/non-AA RRect, dedicated and hairline strokes, four gradients, image sampling, tiled texture, atlas text and clip, ColorFilter, path mask, blend and offscreen clip, Perlin noise, blur/shadow/mask filter, mesh, batch/instancing | 256 |

`coldstart.shader-mix` is not a MotionMark score-compatible scene. It is a
dedicated stress workload for the cold-start `testIntent`. Count units rotate
through 13 deterministic non-YUV motif types; a preview count of 32 still covers every
motif at least twice. The automatic acceptance run uses this scene only.

Scene revision `v7` distributes motif units across the viewport over a
shared animated backdrop instead of concentrating them around the center. Count growth adds real
shader-path diversity while preserving one visual composition. Motifs include
complex and non-AA geometry, tiled and subset texture sampling, ColorFilters,
mask/offscreen paths, noise and blur effects, mesh, and repeated-shape batching.
Animation uses elapsed seconds. Units use a deterministic low-discrepancy layout,
drift within the viewport, and are drawn in atmospheric, effect, structural, and
foreground layers. YUV layouts are excluded from this suite because AOT WebGL
I420 has caused an IOGPU/WindowServer hang; they belong to isolated renderer
diagnostics. Compare only runs using scene version `v7`. The 13 motif dispatch
paths, first-frame expensive effects, and default count remain unchanged.

## Interpretation

- These scenes conserve MotionMark's test intent and visual workload categories,
  but are implemented through TGFX. Their timings are not MotionMark scores and
  must not be compared numerically with upstream MotionMark results.
- The Images case uses image transforms and sampling through TGFX; upstream
  MotionMark Images additionally mutates Canvas 2D `ImageData` pixels.
- Canvas Arcs defaults to eight items and Canvas Lines to sixteen so their first
  preview is responsive at the benchmark's full-DPR backing resolution while
  retaining visible scene detail. Ramp mode lets the controller find the stable
  count; 32 items remain a valid manual stress load.
- Count means active draw objects except in Design, where one unit is one
  multilingual template layer (12 labels). Overlap is intentional; count is not
  a pixel-coverage measurement.
- New Leaves and Suits particles are scattered inside the canvas and reflect
  at its edges. Additional Multiply layers use distinct positions within each
  grid cell. Image orbits stay inside the viewport. Arc sweep and line lengths
  oscillate rather than accumulating against a clamp. Design uses two or three
  columns according to the aspect ratio, with conservative text sizing.
- Every scene remains animated while a run is active. Fixed-load parameters are
  deterministic; wall-clock time, capped at 100 ms after a stall, drives motion.
- Ramp mode uses a bracketed feedback search. Passing and failing counts remain
  explicit in the JSON report, and a run does not settle until the measured
  FPS/p95 boundary is sufficiently narrow.
