# TGFX Benchmark Resource and Dependency Notes

Date: 2026-10-26.

## Runtime resources

The benchmark UI and pages use only local repository resources:

- `web/demo/benchmark-ui.css`
- `web/demo/benchmark-ui.js`
- `../packages/aot-webgl/`
- `../packages/aot-webgpu/`
- `../packages/jit-webgl/`
- `../packages/jit-webgpu/`
- `resources/assets/bridge.jpg`
- `resources/font/NotoSansSC-Regular.otf`
- `web/demo/static/image/motionmark/`

The benchmark UI does not load remote CSS, JavaScript, fonts, icons, images, or analytics.

## External code

The benchmark depends on the repository's vendored TGFX and third-party dependencies.
No package installation is performed by the benchmark runner itself.

## MotionMark

The benchmark includes thirteen 100 px icon assets copied from WebKit MotionMark,
plus the upstream license at `web/demo/static/image/motionmark/LICENSE.txt`.
Scene behavior is reimplemented with TGFX APIs rather than vendoring MotionMark
JavaScript. Source revision and case mapping are recorded in
`docs/BENCHMARK_CASES.md`.
