#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

const repoRoot = path.resolve(__dirname, '../..');
const baseUrl = (process.env.BENCHMARK_URL || 'http://127.0.0.1:8061').replace(/\/+$/, '');
const browserPath = process.env.CHROMIUM_PATH || chromium.executablePath();
const packageIds = (process.env.BENCHMARK_PACKAGES ||
  'aot-webgl,aot-webgpu,jit-webgl,jit-webgpu')
    .split(',').map(value => value.trim()).filter(Boolean);
const count = Number(process.env.COLD_START_COUNT || 256);

function actionableFailureCount(evidence) {
  return Object.entries(evidence?.fallbackCounts || {}).reduce((total, [reason, value]) =>
    reason === 'DeferredTexture' ? total : total + Number(value || 0), 0);
}

async function runPackage(packageId) {
  const identity = JSON.parse(fs.readFileSync(
      path.resolve(repoRoot, `../packages/${packageId}/tgfx-build.json`), 'utf8'));
  const browser = await chromium.launch({
    headless: true,
    executablePath: browserPath,
    args: [
      '--no-sandbox',
      '--enable-unsafe-webgpu',
      '--disable-gpu-shader-disk-cache',
      '--disk-cache-size=1',
    ],
  });
  try {
    const page = await browser.newPage({viewport: {width: 1000, height: 700}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    const query = new URLSearchParams({
      benchmark: '1',
      testIntent: 'cold-start',
      case: 'coldstart.shader-mix',
      count: String(count),
      initialWindowFrames: '30',
      width: '900',
      height: '600',
      recorder: '1',
      backend: identity.backend,
      aot: identity.codegen === 'aot' ? '1' : '0',
      runNonce: `cold-${packageId}-${Date.now()}`,
    });
    await page.goto(`${baseUrl}/packages/${packageId}/index.html?${query}`,
        {waitUntil: 'load', timeout: 30000});
    await page.waitForFunction(() => window.__tgfxBenchmark?.benchmarkSummary !== null,
        null, {timeout: 60000});
    const result = await page.evaluate(() => ({
      summary: window.__tgfxBenchmark.benchmarkSummary,
      runtimeAOT: window.__tgfxBenchmarkView?.getAOTEvidence?.() ?? null,
    }));
    const isolation = await page.evaluate(async () => {
      const nonce = new URLSearchParams(location.search).get('runNonce');
      const response = await fetch(`./tgfx-build.json?runNonce=${encodeURIComponent(nonce)}`, {
        cache: 'no-store',
      });
      return {
        cacheControl: response.headers.get('cache-control'),
        nonce,
        nonceResources: performance.getEntriesByType('resource')
          .map(entry => entry.name).filter(name => name.includes(`runNonce=${nonce}`)).length,
      };
    });
    const summary = result.summary;
    assert.equal(summary.testIntent, 'cold-start');
    assert.equal(summary.executionStatus, 'completed');
    assert.equal(summary.finalSceneCount, count);
    assert.ok(summary.firstSubmitUs > 0);
    assert.ok(summary.coldStartFirstVisibleUs > 0);
    assert.ok(summary.coldStartFramesAfterVisible >= 30);
    assert.ok(summary.sampleCount >= 31);
    assert.equal(summary.warmupFrameCount, 0);
    assert.equal(summary.adaptive, false);
    assert.equal(summary.firstFrame?.available, true);
    assert.equal(summary.firstFrame.reason, null);
    assert.ok(summary.firstFrame.callbackUs > 0);
    assert.ok(summary.firstFrame.sceneDrawUs >= 0);
    assert.ok(summary.firstFrame.flushUs >= 0);
    assert.ok(summary.firstFrame.submitUs >= 0);
    assert.match(isolation.cacheControl || '', /no-store/);
    assert.ok(isolation.nonceResources > 0);
    if (identity.codegen === 'aot') {
      assert.equal(result.runtimeAOT?.available, true);
      assert.equal(result.runtimeAOT?.isLoaded, true);
      assert.equal(actionableFailureCount(result.runtimeAOT), 0);
    } else {
      assert.notEqual(result.runtimeAOT?.isLoaded, true);
    }
    const screenshot = await page.locator('#benchmark').screenshot();
    assert.ok(screenshot.length > 1024);
    assert.deepEqual(errors, []);
    return {
      packageId,
      commit: identity.commit,
      count,
      firstSubmitMs: summary.firstSubmitUs / 1000,
      firstVisibleMs: summary.coldStartFirstVisibleUs / 1000,
      firstFrame: summary.firstFrame,
      initialWindowFrames: summary.coldStartFramesAfterVisible,
      sampleCount: summary.sampleCount,
      screenshotSha256: crypto.createHash('sha256').update(screenshot).digest('hex'),
      runtimeAOT: result.runtimeAOT,
      cacheIsolation: isolation,
      isolationLevel: 'fresh-process',
    };
  } finally {
    await browser.close();
  }
}

async function verifySteadyWarmup(packageId) {
  const identity = JSON.parse(fs.readFileSync(
      path.resolve(repoRoot, `../packages/${packageId}/tgfx-build.json`), 'utf8'));
  const browser = await chromium.launch({
    headless: true,
    executablePath: browserPath,
    args: [
      '--no-sandbox',
      '--enable-unsafe-webgpu',
      '--disable-gpu-shader-disk-cache',
      '--disk-cache-size=1',
    ],
  });
  try {
    const page = await browser.newPage({viewport: {width: 1000, height: 700}});
    const query = new URLSearchParams({
      benchmark: '1',
      testIntent: 'steady-state',
      case: 'coldstart.shader-mix',
      count: '32',
      measurementMs: '500',
      width: '900',
      height: '600',
      recorder: '1',
      backend: identity.backend,
      aot: identity.codegen === 'aot' ? '1' : '0',
      runNonce: `steady-${packageId}-${Date.now()}`,
    });
    await page.goto(`${baseUrl}/packages/${packageId}/index.html?${query}`,
        {waitUntil: 'load', timeout: 30000});
    await page.waitForFunction(() => window.__tgfxBenchmark?.benchmarkSummary !== null,
        null, {timeout: 30000});
    const summary = await page.evaluate(() => window.__tgfxBenchmark.benchmarkSummary);
    assert.equal(summary.testIntent, 'steady-state');
    assert.equal(summary.warmupFrameCount, 10);
    assert.equal(summary.firstFrame?.available, false);
    assert.equal(summary.firstFrame?.reason, 'not-cold-start');
    assert.equal(summary.firstFrame?.callbackUs, null);
    assert.ok(summary.samples[0]?.frameId > 10);
    assert.ok(summary.measurementDurationUs > 0);
    return {
      packageId,
      warmupFrameCount: summary.warmupFrameCount,
      firstMeasuredFrameId: summary.samples[0]?.frameId,
      measurementDurationUs: summary.measurementDurationUs,
    };
  } finally {
    await browser.close();
  }
}

async function main() {
  const packages = {};
  for (const packageId of packageIds) {
    packages[packageId] = await runPackage(packageId);
    console.log(
        `${packageId}: first visible ${packages[packageId].firstVisibleMs.toFixed(1)} ms, ` +
        `${packages[packageId].initialWindowFrames} initial frames`);
  }
  const steadyWarmup = {};
  for (const packageId of packageIds) {
    steadyWarmup[packageId] = await verifySteadyWarmup(packageId);
  }
  const output = {
    schemaVersion: 'tgfx-benchmark.cold-start.v1',
    testIntent: 'cold-start',
    scene: 'coldstart.shader-mix',
    count,
    packages,
    steadyWarmup,
    executionStatus: 'completed',
  };
  const outputPath = path.join(repoRoot, 'results', 'cold-start.json');
  fs.mkdirSync(path.dirname(outputPath), {recursive: true});
  fs.writeFileSync(outputPath, `${JSON.stringify(output, null, 2)}\n`, 'utf8');
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
