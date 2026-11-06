#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const {createHash} = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

const baseUrl = process.env.BENCHMARK_URL || 'http://127.0.0.1:8061/benchmark-ui.html';
const resultsDir = path.resolve(__dirname, '../../results/shader-mix-ui');
const resultTag = (process.env.BENCHMARK_RESULT_TAG || '').replace(/[^a-z0-9._-]+/gi, '-');
const packages = (process.env.BENCHMARK_PACKAGES ||
  'aot-webgl,aot-webgpu,jit-webgl,jit-webgpu').split(',');

async function waitForPreview(page, packageId) {
  await page.waitForFunction(expected => {
    const win = document.querySelector('#scene-frame').contentWindow;
    return win.location.href.includes(`/packages/${expected}/`) &&
      win.location.search.includes('case=coldstart.shader-mix') &&
      win.__tgfxBenchmark?.firstDrawAt != null;
  }, packageId, {timeout: 30000});
}

async function inspectPixels(page, screenshot) {
  return page.evaluate(async data => {
    const image = new Image();
    image.src = `data:image/png;base64,${data}`;
    await image.decode();
    const canvas = document.createElement('canvas');
    canvas.width = image.width;
    canvas.height = image.height;
    const context = canvas.getContext('2d');
    context.drawImage(image, 0, 0);
    const {data: pixels} = context.getImageData(0, 0, image.width, image.height);
    let colorful = 0;
    let bright = 0;
    for (let index = 0; index < pixels.length; index += 4) {
      const values = [pixels[index], pixels[index + 1], pixels[index + 2]];
      if (Math.max(...values) - Math.min(...values) > 40) colorful++;
      if (Math.max(...values) > 180) bright++;
    }
    return {colorfulFraction: colorful / (pixels.length / 4),
      brightFraction: bright / (pixels.length / 4)};
  }, screenshot.toString('base64'));
}

async function inspectReportPreview(page) {
  return page.evaluate(async () => {
    const preview = document.querySelector('#report-preview');
    if (preview.classList.contains('hidden') || !preview.src) {
      return {hidden: true, opaqueFraction: 0, colorfulFraction: 0};
    }
    await preview.decode();
    const canvas = document.createElement('canvas');
    canvas.width = preview.naturalWidth;
    canvas.height = preview.naturalHeight;
    const context = canvas.getContext('2d', {willReadFrequently: true});
    context.drawImage(preview, 0, 0);
    const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
    let opaque = 0;
    let colorful = 0;
    for (let index = 0; index < pixels.length; index += 4) {
      const values = [pixels[index], pixels[index + 1], pixels[index + 2]];
      if (pixels[index + 3] > 0) opaque++;
      if (Math.max(...values) - Math.min(...values) > 40) colorful++;
    }
    const count = pixels.length / 4;
    return {
      hidden: false,
      width: canvas.width,
      height: canvas.height,
      opaqueFraction: opaque / count,
      colorfulFraction: colorful / count,
    };
  });
}

async function checkPackage(browser, packageId) {
  const page = await browser.newPage({viewport: {width: 1440, height: 900}, deviceScaleFactor: 2});
  try {
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(baseUrl);
    console.log(`${packageId}: UI loaded`);
    await page.waitForFunction(() => document.querySelector('#scene-frame')
      .contentWindow.__tgfxBenchmark?.firstDrawAt != null);
    console.log(`${packageId}: default preview ready`);
    const identityPath = path.resolve(__dirname, `../../../packages/${packageId}/tgfx-build.json`);
    const identity = JSON.parse(fs.readFileSync(identityPath, 'utf8'));
    const engineButton = page.locator(`[data-engine-mode="${identity.codegen}"]`);
    if (await engineButton.getAttribute('aria-pressed') !== 'true') {
      await engineButton.click();
    }
    await page.locator('#backend').selectOption(identity.backend);
    await page.locator('#package-source').selectOption(packageId);
    await page.locator('#scene').selectOption('coldstart.shader-mix');
    console.log(`${packageId}: package selected`);
    assert.equal(await page.locator('[data-test-intent="cold-start"]').getAttribute('aria-pressed'), 'true');
    assert.equal(await page.locator('[data-view="report"]').isDisabled(), true);
    await waitForPreview(page, packageId);
    console.log(`${packageId}: Shader Mix preview ready`);
    assert.equal(await page.evaluate(() => JSON.parse(document.querySelector('#scene-frame')
      .contentWindow.__tgfxBenchmarkView.getBenchmarkLiveStatsJson()).sceneCount), 32);

    const geometry = await page.evaluate(() => {
      const stage = document.querySelector('#scene-stage');
      const win = document.querySelector('#scene-frame').contentWindow;
      const canvas = win.document.querySelector('#benchmark');
      return {stageWidth: stage.clientWidth, stageHeight: stage.clientHeight,
        width: canvas.clientWidth, height: canvas.clientHeight,
        pixelWidth: canvas.width, pixelHeight: canvas.height, dpr: win.devicePixelRatio};
    });
    assert.ok(Math.abs(geometry.stageWidth - geometry.width) <= 1);
    assert.ok(Math.abs(geometry.stageHeight - geometry.height) <= 1);
    assert.ok(Math.abs(geometry.pixelWidth - geometry.width * geometry.dpr) <= 1);
    assert.ok(Math.abs(geometry.pixelHeight - geometry.height * geometry.dpr) <= 1);
    const frame = page.frameLocator('#scene-frame');
    console.log(`${packageId}: first preview capture`);
    const first = await frame.locator('#benchmark').screenshot({scale: 'css'});
    await page.waitForTimeout(500);
    console.log(`${packageId}: second preview capture`);
    const second = await frame.locator('#benchmark').screenshot({scale: 'css'});
    assert.notEqual(createHash('sha256').update(first).digest('hex'),
      createHash('sha256').update(second).digest('hex'), `${packageId}: static preview`);
    console.log(`${packageId}: animation hash validated`);
    const pixels = await inspectPixels(page, second);
    assert.ok(pixels.colorfulFraction > 0.05, `${packageId}: blank or colorless canvas`);
    assert.ok(pixels.brightFraction > 0.005, `${packageId}: missing image/text highlights`);
    await page.screenshot({path: path.join(resultsDir, `${packageId}-preview.png`)});
    console.log(`${packageId}: preview pixels validated`);

    const runs = [];
    for (const count of [128, 256]) {
      console.log(`${packageId}: cold-start ${count} starting`);
      if (runs.length) {
        await page.locator('[data-view="scene"]').click();
        await waitForPreview(page, packageId);
      }
      await page.locator('#count').fill(String(count));
      await page.locator('#run').click();
      await page.locator('#report-view').waitFor({state: 'visible', timeout: 30000});
      assert.equal(await page.locator('[data-view="report"]').isEnabled(), true);
      const preview = await inspectReportPreview(page);
      assert.equal(preview.hidden, false, `${packageId}: completion preview is hidden`);
      assert.ok(preview.opaqueFraction > 0.5,
        `${packageId}: completion preview is transparent`);
      assert.ok(preview.colorfulFraction > 0.02,
        `${packageId}: completion preview is blank`);
      const run = await page.evaluate(() => ({
        summary: document.querySelector('#scene-frame').contentWindow.__tgfxBenchmark.benchmarkSummary,
        nonce: new URL(document.querySelector('#scene-frame').src).searchParams.get('runNonce'),
        firstVisible: document.querySelector('#cold-first-visible').textContent,
        countLabel: document.querySelector('#cold-count-label').textContent,
        reportCount: Number(document.querySelector('#cold-count').textContent),
        historyCount: Number(document.querySelector('#recent-runs tr td[data-column="count"]')
          .textContent),
        historyHeaders: [...document.querySelectorAll('#recent-runs-head th')]
          .map(cell => cell.textContent),
        historyTitle: document.querySelector('#session-history-title').textContent,
        subtitle: document.querySelector('#report-subtitle').textContent,
        shaderGroups: document.querySelector('#cold-shader-groups').textContent,
        chartTitle: document.querySelector('#cold-frame-chart')
          .closest('section').querySelector('h3').textContent,
        chartNote: document.querySelector('#cold-note').textContent,
        metricLabels: [...document.querySelectorAll('#cold-report .metric-grid .metric span')]
          .map(cell => cell.textContent),
        refreshLabel: document.querySelector('#live-refresh')
          .closest('div').querySelector('dt').textContent,
        visibleText: document.body.innerText,
        error: document.querySelector('#error').textContent,
        hitSummary: document.querySelector('#scene-frame').contentWindow
          .__tgfxBenchmarkView.getAOTEvidence().hitSummary,
      }));
      assert.equal(run.summary.executionStatus, 'completed');
      assert.equal(run.summary.testIntent, 'cold-start');
      assert.equal(run.summary.finalSceneCount, count);
      assert.equal(run.countLabel, '初始绘制单元');
      assert.equal(run.reportCount, count);
      assert.equal(run.historyCount, count);
      assert.equal(run.historyTitle, '首帧测试会话记录');
      assert.ok(run.historyHeaders.includes('执行路径'));
      assert.ok(run.historyHeaders.includes('首次命令提交'));
      assert.ok(run.historyHeaders.includes('首帧后下一次刷新回调'));
      assert.ok(run.historyHeaders.includes('首帧 CPU 回调'));
      assert.ok(run.historyHeaders.includes('后续 30 帧 p95（ms）'));
      assert.ok(run.historyHeaders.includes('AOT 证据'));
      assert.ok(!run.historyHeaders.includes('帧间隔 p95'));
      assert.equal(run.summary.warmupFrameCount, 0);
      assert.ok(run.summary.coldStartFramesAfterVisible >= 30);
      assert.notEqual(run.firstVisible, '-');
      assert.match(run.shaderGroups, /AA 圆角矩形/);
      assert.match(run.shaderGroups, /Blur \/ Shadow \/ MaskFilter/);
      assert.match(run.shaderGroups, /Batch \/ Instancing/);
      assert.match(run.chartTitle, /毫秒/);
      assert.match(run.chartNote, /帧序号/);
      assert.match(run.chartNote, /毫秒（ms）/);
      assert.ok(run.metricLabels.includes('后续 30 帧 p95（ms）'));
      assert.ok(run.metricLabels.includes('首帧后采样帧数（帧）'));
      assert.doesNotMatch(run.visibleText, /\brAF\b/);
      assert.equal(run.refreshLabel, '浏览器刷新上限');
      assert.ok(identity.codegen !== 'aot' ||
        (Array.isArray(run.hitSummary) && run.hitSummary.length > 0),
      `${packageId}: AOT evidence did not expose shader-family hits`);
      assert.match(run.subtitle, /首帧测试/);
      assert.equal(run.error, '');
      runs.push({count, nonce: run.nonce, firstVisible: run.firstVisible,
        initialWindowFrames: run.summary.coldStartFramesAfterVisible, preview});
      console.log(`${packageId}: cold-start ${count} completed`);
    }
    assert.notEqual(runs[0].nonce, runs[1].nonce);
    await page.screenshot({path: path.join(resultsDir, `${packageId}-report.png`)});
    console.log(`${packageId}: report validated`);

    await page.locator('[data-view="scene"]').click();
    await waitForPreview(page, packageId);
    await page.setViewportSize({width: 390, height: 844});
    await page.waitForTimeout(1000);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true);
    await page.screenshot({path: path.join(resultsDir, `${packageId}-mobile.png`)});
    console.log(`${packageId}: mobile validated`);
    assert.deepEqual(errors, []);
    console.log(`${packageId}: animated preview, full DPR, two cold runs, report and mobile passed`);
    return {packageId, geometry, pixels, runs};
  } finally {
    await page.close();
  }
}

async function checkInitializationFailure(browser) {
  const page = await browser.newPage();
  try {
    await page.route('**/packages/*/index.html?**', async route => {
      const response = await route.fetch();
      const body = await response.text();
      const injected = '<script>addEventListener("unhandledrejection", () => {' +
        'window.__tgfxBenchmark ||= {};' +
        'window.__tgfxBenchmark.initializationError = "injected startup failure";' +
        '}); onload = async () => {throw new Error("injected startup failure")};</script>';
      await route.fulfill({response, body: body.replace(/<script type="module"[^>]*><\/script>/, injected)});
    });
    await page.goto(baseUrl);
    await page.locator('#scene').selectOption('coldstart.shader-mix');
    await page.waitForFunction(() => document.querySelector('#error').textContent
      .includes('injected startup failure'));
    await page.locator('#run').click();
    await page.waitForFunction(() => document.querySelector('#error').textContent
      .includes('injected startup failure') &&
      document.querySelector('#cancel').classList.contains('hidden'));
    assert.equal(await page.locator('#cancel').isVisible(), false);
    assert.equal(await page.locator('#run').isVisible(), true);
    assert.equal(await page.locator('[data-view="report"]').isDisabled(), true);
    await page.unrouteAll();
    await page.locator('#run').click();
    await page.locator('#report-view').waitFor({state: 'visible', timeout: 30000});
    console.log('startup failure: immediate error, no false report and retry passed');
  } finally {
    await page.close();
  }
}

async function main() {
  fs.mkdirSync(resultsDir, {recursive: true});
  const browser = await chromium.launch({headless: true,
    executablePath: process.env.CHROMIUM_PATH || chromium.executablePath(),
    args: ['--enable-unsafe-webgpu', '--disable-gpu-shader-disk-cache']});
  try {
    const results = [];
    for (const packageId of packages) results.push(await checkPackage(browser, packageId));
    await checkInitializationFailure(browser);
    const acceptanceName = resultTag ? `acceptance-${resultTag}.json` : 'acceptance.json';
    fs.writeFileSync(path.join(resultsDir, acceptanceName),
      JSON.stringify({sceneVersion: 'v7', results}, null, 2) + '\n');
  } finally {
    await browser.close();
  }
}

main().catch(error => {console.error(error); process.exitCode = 1;});
