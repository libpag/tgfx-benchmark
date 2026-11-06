#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.CHROMIUM_PATH || chromium.executablePath(),
    args: ['--enable-unsafe-webgpu'],
  });
  try {
    for (const backend of ['webgl', 'webgpu']) {
      const page = await browser.newPage({
        viewport: {width: 390, height: 844}, deviceScaleFactor: 2,
      });
      await page.goto(process.env.BENCHMARK_URL || 'http://127.0.0.1:8061/benchmark-ui.html');
      await page.locator('#backend').selectOption(backend);
      await page.locator('#scene').selectOption('motionmark.canvas-arcs');
      await page.locator('#count').fill('32');
      await page.locator('#count').dispatchEvent('change');
      await page.locator('#run').click();
      await page.waitForFunction(() => !document.querySelector('#stop').disabled &&
        Number(document.querySelector('#live-samples').textContent) > 2);
      await page.waitForFunction(() =>
        document.querySelector('#scene-phase').textContent === '估算触边',
      null, {timeout: 30000});
      assert.equal(await page.locator('#error').textContent(), '',
        `${backend}: telemetry changed stage geometry`);
      assert.equal(await page.evaluate(() =>
        document.documentElement.scrollWidth <= innerWidth), true);
      assert.equal(await page.locator('#stop').textContent(), '结束并查看触边报告');
      await page.locator('#stop').click();
      await page.locator('#report-view').waitFor({state: 'visible'});
      const stopped = await page.evaluate(() => ({
        finalCount: document.querySelector('#scene-frame').contentWindow
          .__tgfxBenchmark.benchmarkSummary.finalSceneCount,
        header: document.querySelector('#report-subtitle').textContent,
        rampFinal: Number(document.querySelector('#ramp-final').textContent),
        historyCount: Number(document.querySelector('#recent-runs tr td[data-column="count"]')
          .textContent),
        historyStatus: document.querySelector('#recent-runs tr td[data-column="status"]').textContent,
        historyMode: document.querySelector('#recent-runs tr td[data-column="mode"]').textContent,
        historyFirstSubmit: document.querySelector('#recent-runs tr td[data-column="firstSubmit"]')
          .textContent,
        historyWarmup: document.querySelector('#recent-runs tr td[data-column="warmup"]').textContent,
        historyCallbackP95: document.querySelector('#recent-runs tr td[data-column="callbackP95"]')
          .textContent,
        historyRuntime: document.querySelector('#recent-runs tr td[data-column="runtime"]')
          .textContent,
        thresholdReached: document.querySelector('#scene-frame').contentWindow
          .__tgfxBenchmark.benchmarkSummary.adaptiveThresholdReached,
      }));
      assert.match(stopped.header, new RegExp(`${stopped.finalCount} 个对象`));
      assert.match(stopped.header, /估算触边/);
      assert.equal(stopped.thresholdReached, true);
      assert.equal(stopped.rampFinal, stopped.finalCount);
      assert.equal(stopped.historyCount, stopped.finalCount);
      assert.equal(stopped.historyStatus, '估算触边');
      assert.ok(['AOT', 'JIT'].includes(stopped.historyMode));
      assert.notEqual(stopped.historyFirstSubmit, '-');
      assert.match(stopped.historyWarmup, /^10 帧$/);
      assert.notEqual(stopped.historyCallbackP95, '-');
      assert.ok(stopped.historyRuntime === 'JIT' || stopped.historyRuntime.includes('hit '));
      assert.ok(Number(await page.locator('#metric-samples').textContent()) > 0,
        `${backend}: report must retain the latest same-count window`);
      assert.notEqual(await page.locator('#metric-p95').textContent(), '-');
      const warning = await page.locator('#report-warning').textContent();
      assert.equal(warning, '', `${backend}: manual ramp stop must not show a warning`);
      assert.match(await page.locator('#ramp-verdict').textContent(),
        /估算容量/);
      assert.notEqual(await page.locator('#ramp-last-fps').textContent(), '-');
      assert.notEqual(await page.locator('#ramp-last-p95').textContent(), '-');
      await page.locator('[data-view="scene"]').click();
      await page.locator('#run').click();
      await page.waitForFunction(() => {
        const win = document.querySelector('#scene-frame').contentWindow;
        return win.__tgfxBenchmark?.firstDrawAt != null &&
          JSON.parse(win.__tgfxBenchmarkView.getBenchmarkLiveStatsJson()).active;
      });
      await page.evaluate(() => {
        const win = document.querySelector('#scene-frame').contentWindow;
        win.dispatchEvent(new win.ErrorEvent('error', {message: 'injected runtime failure'}));
      });
      await page.waitForFunction(() =>
        document.querySelector('#error').textContent.includes('injected runtime failure'));
      assert.equal(await page.locator('#cancel').isVisible(), false);
      await page.locator('#run').click();
      await page.waitForFunction(() => !document.querySelector('#stop').disabled &&
        Number(document.querySelector('#live-samples').textContent) > 2);
      await page.setViewportSize({width: 480, height: 844});
      await page.waitForFunction(() =>
        document.querySelector('#error').textContent.includes('尺寸发生变化'));
      assert.equal(await page.locator('#cancel').isVisible(), false);
      console.log(`${backend}: narrow layout, unsettled stop, runtime failure and resize passed`);
      await page.close();
    }
  } finally {
    await browser.close();
  }
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
