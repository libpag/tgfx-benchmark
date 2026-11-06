#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const os = require('node:os');
const path = require('node:path');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

async function frameConfig(page) {
  return page.frameLocator('#scene-frame').locator('body').evaluate(() =>
    window.__tgfxBenchmarkConfig ?? null);
}

async function main() {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.CHROMIUM_PATH || chromium.executablePath(),
  });
  const baseUrl = (process.env.BENCHMARK_URL || 'http://127.0.0.1:8061').replace(/\/+$/, '');
  const page = await browser.newPage({viewport: {width: 1440, height: 900}});
  const pageErrors = [];
  page.on('pageerror', error => pageErrors.push(error.message));
  try {
    await page.goto(`${baseUrl}/benchmark-ui.html`);
    await page.waitForFunction(() => document.querySelectorAll('#scene option').length === 13);
    assert.equal(await page.locator('#scene option[value^="legacy."]').count(), 5);
    assert.equal(await page.locator('[data-scene^="legacy."]').count(), 5);

    for (const [caseId, count] of [
      ['legacy.particles-rect', '600'],
      ['legacy.particles-circle', '600'],
      ['legacy.particles-oval', '600'],
      ['legacy.particles-rrect', '600'],
      ['legacy.particles-star', '300'],
    ]) {
      await page.locator('#scene').selectOption(caseId);
      assert.equal(await page.locator('#count').inputValue(), count);
      await page.waitForFunction(expected => {
        const frame = document.querySelector('#scene-frame');
        return frame.contentWindow?.__tgfxBenchmarkConfig?.caseId === expected;
      }, caseId);
    }

    await page.locator('#scene').selectOption('legacy.particles-rect');
    await page.waitForFunction(() =>
      !document.querySelector('#particle-options').classList.contains('hidden'));
    assert.equal(await page.locator('#count').inputValue(), '600');
    assert.equal(await page.locator('#antialias').inputValue(), 'true');
    assert.equal(await page.locator('#stroke').inputValue(), 'false');

    await page.locator('#antialias').selectOption('false');
    await page.locator('#stroke').selectOption('true');
    await page.waitForFunction(() => {
      const frame = document.querySelector('#scene-frame');
      const config = frame.contentWindow?.__tgfxBenchmarkConfig;
      return config?.caseId === 'legacy.particles-rect' &&
        config.antialias === false && config.stroke === true;
    });
    assert.match(await page.locator('#scene-frame').getAttribute('src'),
      /case=legacy\.particles-rect/);
    assert.match(await page.locator('#scene-frame').getAttribute('src'), /aa=0/);
    assert.match(await page.locator('#scene-frame').getAttribute('src'), /stroke=1/);
    const previewConfig = await frameConfig(page);
    assert.equal(previewConfig.caseId, 'legacy.particles-rect');
    assert.equal(previewConfig.count, 600);
    assert.equal(previewConfig.backend, 'webgl');
    assert.equal(previewConfig.antialias, false);
    assert.equal(previewConfig.stroke, true);

    await page.locator('[data-run-mode="manual"]').click();
    await page.locator('#run').click();
    await page.waitForFunction(() => {
      const frame = document.querySelector('#scene-frame');
      return frame.src.includes('benchmark=1') && frame.src.includes('manual=1') &&
        frame.src.includes('aa=0') && frame.src.includes('stroke=1');
    });
    await page.waitForFunction(() => {
      const view = document.querySelector('#scene-frame').contentWindow?.__tgfxBenchmarkView;
      return view && typeof view.getBenchmarkLiveStatsJson === 'function' &&
        JSON.parse(view.getBenchmarkLiveStatsJson()).active === true;
    }, null, {timeout: 30000});
    await page.waitForFunction(() => {
      const frame = document.querySelector('#scene-frame').contentWindow;
      const canvas = frame?.document.querySelector('#benchmark');
      return frame?.__tgfxBenchmark?.firstDrawAt != null && canvas?.clientWidth > 300;
    }, null, {timeout: 30000});
    await page.waitForTimeout(500);
    await page.screenshot({
      path: process.env.LEGACY_UI_SCREENSHOT ||
        path.join(os.tmpdir(), 'tgfx-legacy-ui-check.png'),
      fullPage: true,
    });
    await page.locator('#cancel').click();
    assert.deepEqual(pageErrors, []);
    console.log('legacy UI: scenes, options, preview and manual run configuration passed');
  } finally {
    await browser.close();
  }
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
