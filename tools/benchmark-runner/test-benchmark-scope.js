#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '../..');
const read = relativePath => fs.readFileSync(path.join(root, relativePath), 'utf8');

const officialScenes = [
  'coldstart.shader-mix',
  'motionmark.canvas-arcs',
  'motionmark.canvas-lines',
  'motionmark.design',
  'motionmark.images',
  'motionmark.leaves',
  'motionmark.multiply',
  'motionmark.suits',
];
const legacyScenes = [
  'legacy.particles-circle',
  'legacy.particles-oval',
  'legacy.particles-rect',
  'legacy.particles-rrect',
  'legacy.particles-star',
];
const expectedUiScenes = [...officialScenes, ...legacyScenes].sort();

const sceneHtml = read('web/demo/benchmark-ui.html');
const sceneSelect = sceneHtml.match(/<select id="scene">([\s\S]*?)<\/select>/);
assert.ok(sceneSelect, 'scene selector was not found');
const uiScenes = [...sceneSelect[1].matchAll(/<option value="([^"]+)"/g)]
    .map(match => match[1])
    .sort();
assert.deepEqual(uiScenes, expectedUiScenes, 'benchmark UI scene scope changed');
assert.match(sceneSelect[1], /<optgroup label="原版 TGFX">/,
    'the original TGFX particle group is missing');
for (const controlId of ['particle-options', 'antialias', 'stroke']) {
  assert.match(sceneHtml, new RegExp(`id="${controlId}"`),
      `original benchmark control is missing: ${controlId}`);
}

const commonSource = read('web/demo/common.ts');
const uiSource = read('web/demo/benchmark-ui.js');
assert.match(commonSource, /setAntiAlias\(antialias\)/,
    'web runtime does not apply the original antialias option');
assert.match(commonSource, /setStroke\(stroke\)/,
    'web runtime does not apply the original stroke option');

const viewSource = read('web/demo/src/TGFXBaseView.cpp');
for (const field of ['callbackUs', 'sceneDrawUs', 'flushUs', 'submitUs']) {
  assert.match(viewSource, new RegExp(`\\\\"${field}\\\\"`),
      `first-frame summary field is missing: ${field}`);
}
assert.doesNotMatch(sceneHtml, /id="cold-phase-(?:draw|flush|submit|window)"/,
    'first-frame phase diagnostics must stay out of the compact UI');
assert.doesNotMatch(uiSource, /\[['"](?:firstDraw|firstFlush)['"]/,
    'first-frame phase diagnostics must stay out of the compact UI history');

const benchSource = read('src/benchmark/MotionMarkBenches.cpp');
assert.match(benchSource, /constexpr size_t kShaderMixMotifCount = 13;/,
    'Shader Mix must contain the 13 approved non-YUV motifs');
assert.doesNotMatch(benchSource, /YUVData|DrawI420|DrawNV12/,
    'YUV hardware diagnostics must stay out of the official benchmark scene');

for (const historyField of [
  "['mode', '执行路径']",
  "['firstSubmit', '首次 submit']",
  "['callbackP95', 'CPU callback p95']",
  "['runtime', 'AOT 证据']",
]) {
  assert.ok(uiSource.includes(historyField),
      `history field is missing: ${historyField}`);
}

const coverageSource = read('tools/benchmark-runner/audit-aot-coverage.js');
const coverageBlock = coverageSource.match(/const scenes = \[([\s\S]*?)\n\];/);
assert.ok(coverageBlock, 'AOT coverage scene list was not found');
const coverageScenes = [...coverageBlock[1].matchAll(/\['([^']+)',\s*\d+\]/g)]
    .map(match => match[1])
    .sort();
assert.deepEqual(coverageScenes, officialScenes, 'AOT coverage must match the official UI scope');

const scopeDoc = read('docs/BENCHMARK_SCOPE.md');
for (const scene of officialScenes) {
  assert.match(scopeDoc, new RegExp(scene.replaceAll('.', '\\.')),
      `${scene} is missing from BENCHMARK_SCOPE.md`);
}

for (const removedPath of [
  'tools/benchmark-contract/README.md',
  'tools/benchmark-contract/schemas/run-config.schema.json',
  'tools/benchmark-contract/validate.js',
  'tools/benchmark-runner/audit-load.js',
  'tools/benchmark-runner/summarize-stage4.js',
  'tools/benchmark-runner/run-stage4.js',
  'web/demo/static/css/browser-alert.css',
  'web/demo/static/css/index.css',
  'web/demo/static/css/progress-bar.css',
  'web/demo/static/js/eventHandling.js',
  'web/demo/static/js/locale/en_US.js',
  'web/demo/static/js/locale/zh_CN.js',
  'web/demo/static/sideBarConfigParam.json',
]) {
  assert.equal(fs.existsSync(path.join(root, removedPath)), false,
      `${removedPath} must remain removed`);
}

for (const currentPath of [
  'tools/benchmark-runner/run-web-case.js',
  'tools/benchmark-runner/test-shader-mix-ui.js',
  'tools/benchmark-runner/test-ui-recovery.js',
]) {
  assert.equal(fs.existsSync(path.join(root, currentPath)), true,
      `${currentPath} is missing`);
}

console.log('benchmark scope: UI, Shader Mix, runner and AOT coverage are converged');
