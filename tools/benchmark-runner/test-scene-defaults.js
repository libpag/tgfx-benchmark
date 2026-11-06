#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

async function selectPackage(page, engineMode, backend) {
  const engineButton = page.locator(`[data-engine-mode="${engineMode}"]`);
  if (await engineButton.getAttribute('aria-pressed') !== 'true') {
    await engineButton.click();
  }
  if (await page.locator('#backend').inputValue() !== backend) {
    await page.locator('#backend').selectOption(backend);
  }
  const packageId = `${engineMode}-${backend}`;
  await page.locator('#package-source').selectOption(packageId);
  await page.waitForFunction(expectedPackage => {
    const frame = document.querySelector('#scene-frame');
    const identity = document.querySelector('#engine-identity').textContent;
    return frame.contentWindow?.location.href.includes(`/packages/${expectedPackage}/`) &&
      identity.includes(expectedPackage.includes('aot-') ? 'AOT' : 'JIT');
  }, packageId);
}

async function main() {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.CHROMIUM_PATH || chromium.executablePath(),
    args: ['--enable-unsafe-webgpu'],
  });
  const baseUrl = (process.env.BENCHMARK_URL || 'http://127.0.0.1:8061').replace(/\/+$/, '');
  const engineModes = (process.env.BENCHMARK_ENGINE_MODES || 'aot,jit')
      .split(',').map(value => value.trim()).filter(Boolean);
  try {
    for (const engineMode of engineModes) {
      for (const backend of ['webgl', 'webgpu']) {
        const page = await browser.newPage({viewport: {width: 1440, height: 900}});
        await page.goto(`${baseUrl}/benchmark-ui.html`);
        await selectPackage(page, engineMode, backend);
        assert.equal(await page.locator('#scene option[value="motionmark.paths"]').count(), 0);
        assert.equal(await page.locator('#scene option[value^="legacy."]').count(), 5);
        assert.equal(await page.locator('#scene option').count(), 13);
        for (const [scene, expected] of [
          ['coldstart.shader-mix', '256'],
          ['legacy.particles-rect', '600'],
          ['legacy.particles-star', '300'],
          ['motionmark.canvas-arcs', '8'], ['motionmark.canvas-lines', '16'],
        ]) {
          if (!scene.startsWith('coldstart.')) {
            await page.locator('[data-test-intent="steady-state"]').click();
          }
          await page.locator('#scene').selectOption(scene);
          assert.equal(await page.locator('#count').inputValue(), expected,
            `${engineMode}/${backend}/${scene}: unexpected default`);
        }
        await page.close();

        for (const [scene, expected] of [
          ['coldstart.shader-mix', 256],
          ['motionmark.canvas-arcs', 8], ['motionmark.canvas-lines', 16],
          ['legacy.particles-rect', 600],
        ]) {
          const directPage = await browser.newPage({viewport: {width: 1000, height: 700}});
          const packageId = `${engineMode}-${backend}`;
          await directPage.goto(`${baseUrl}/packages/${packageId}/index.html?case=${scene}&fluid=1`);
          await directPage.waitForFunction(() => {
            const view = window.__tgfxBenchmarkView;
            return view && window.__tgfxBenchmark?.firstDrawAt != null;
          });
          const count = await directPage.evaluate(() =>
            JSON.parse(window.__tgfxBenchmarkView.getBenchmarkLiveStatsJson()).sceneCount);
          assert.equal(count, expected,
            `${engineMode}/${backend}/${scene}: direct URL used a non-scene default`);
          await directPage.close();
        }
        console.log(`${engineMode}/${backend}: scene defaults and direct URLs passed`);
      }
    }
  } finally {
    await browser.close();
  }
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
