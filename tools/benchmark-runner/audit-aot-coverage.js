#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

const repoRoot = path.resolve(__dirname, '../..');
const baseUrl = (process.env.BENCHMARK_URL || 'http://127.0.0.1:8061').replace(/\/+$/, '');
const packageIds = (process.env.BENCHMARK_AOT_PACKAGES || 'aot-webgl,aot-webgpu')
    .split(',').map(value => value.trim()).filter(Boolean);
const browserPath = process.env.CHROMIUM_PATH || chromium.executablePath();
const scenes = [
  ['coldstart.shader-mix', 256],
  ['motionmark.multiply', 512],
  ['motionmark.canvas-arcs', 8],
  ['motionmark.leaves', 128],
  ['motionmark.canvas-lines', 16],
  ['motionmark.images', 64],
  ['motionmark.design', 8],
  ['motionmark.suits', 24],
];

function actionableFailureCount(evidence) {
  return Object.entries(evidence.fallbackCounts || {}).reduce((total, [reason, count]) =>
    reason === 'DeferredTexture' ? total : total + Number(count || 0), 0);
}

async function main() {
  const browser = await chromium.launch({
    headless: true,
    executablePath: browserPath,
    args: ['--no-sandbox', '--enable-unsafe-webgpu'],
  });
  const output = {
    schemaVersion: 'tgfx-benchmark.aot-coverage.v1',
    baseUrl,
    packages: {},
    executionStatus: 'completed',
  };
  try {
    for (const packageId of packageIds) {
      output.packages[packageId] = {};
      for (const [scene, count] of scenes) {
        const page = await browser.newPage({viewport: {width: 900, height: 600}});
        try {
          const query = new URLSearchParams({
            case: scene,
            count: String(count),
            fluid: '1',
            aot: '1',
          });
          await page.goto(`${baseUrl}/packages/${packageId}/index.html?${query}`);
          await page.waitForFunction(() =>
            window.__tgfxBenchmark?.readyAt != null, null, {timeout: 30000});
          await page.waitForTimeout(500);
          const evidence = await page.evaluate(() =>
            window.__tgfxBenchmarkView.getAOTEvidence());
          const result = {
            available: evidence.available === true,
            isLoaded: evidence.isLoaded === true,
            hitCount: evidence.hitCount,
            missCount: evidence.missCount,
            failureCount: actionableFailureCount(evidence),
            fallbackCounts: evidence.fallbackCounts || {},
          };
          output.packages[packageId][scene] = result;
          console.log(
              `${packageId}/${scene}: failures=${result.failureCount}, hits=${result.hitCount}`);
        } finally {
          await page.close();
        }
      }
    }
  } finally {
    await browser.close();
  }

  const failures = Object.values(output.packages).flatMap(sceneResults =>
    Object.entries(sceneResults)
        .filter(([, result]) => !result.available || !result.isLoaded ||
          result.hitCount <= 0 || result.failureCount > 0)
        .map(([scene]) => scene));
  output.validityStatus = failures.length === 0 ? 'valid' : 'invalid';
  output.invalidSceneCount = failures.length;
  const outputPath = path.join(repoRoot, 'results', 'aot-package-coverage.json');
  fs.mkdirSync(path.dirname(outputPath), {recursive: true});
  fs.writeFileSync(outputPath, `${JSON.stringify(output, null, 2)}\n`, 'utf8');
  if (failures.length > 0) {
    throw new Error(`AOT coverage failures: ${failures.join(', ')}`);
  }
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
