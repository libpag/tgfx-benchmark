#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const {createHash} = require('node:crypto');
const {mkdirSync, writeFileSync} = require('node:fs');
const path = require('node:path');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

async function main() {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.CHROMIUM_PATH || chromium.executablePath(),
    args: ['--no-sandbox', '--enable-unsafe-webgpu'],
  });
  const results = [];
  try {
    const page = await browser.newPage({viewport: {width: 1440, height: 900}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(process.env.BENCHMARK_URL || 'http://127.0.0.1:8061/benchmark-ui.html');
    let scenes = await page.locator('#scene option').evaluateAll(options =>
      options.map(option => option.value));
    if (process.env.ANIMATION_SCENES) scenes = process.env.ANIMATION_SCENES.split(',');
    for (const backend of (process.env.BENCHMARK_BACKENDS || 'webgl,webgpu').split(',')) {
      await page.locator('#backend').selectOption(backend);
      for (const scene of scenes) {
        await page.locator('#scene').selectOption(scene);
        await page.waitForFunction(expectedScene => {
          const win = document.querySelector('#scene-frame').contentWindow;
          return win.location.search.includes(`case=${expectedScene}`) &&
            win.__tgfxBenchmark?.firstDrawAt != null;
        }, scene, {timeout: 30000});
        await page.waitForTimeout(600);
        const frame = page.frameLocator('#scene-frame');
        const hashes = [];
        for (let i = 0; i < 4; ++i) {
          const screenshot = await frame.locator('canvas').screenshot();
          hashes.push(createHash('sha256').update(screenshot).digest('hex'));
          if (i === 0 && process.env.ANIMATION_SCREENSHOTS) {
            mkdirSync(process.env.ANIMATION_SCREENSHOTS, {recursive: true});
            writeFileSync(path.join(process.env.ANIMATION_SCREENSHOTS,
              `${backend}-${scene}.png`), screenshot);
          }
          await page.waitForTimeout(250);
        }
        const result = await page.evaluate(() => {
          const win = document.querySelector('#scene-frame').contentWindow;
          return JSON.parse(win.__tgfxBenchmarkView.getBenchmarkLiveStatsJson());
        });
        assert.equal(result.imageAvailable, true, `${backend}: benchmark image is missing`);
        assert.ok(result.imageCount >= 14, `${backend}: MotionMark image assets are missing`);
        const moving = new Set(hashes).size === hashes.length;
        results.push({backend, scene, moving, hashes, fps: result.currentFps,
          actualCount: result.sceneCount});
        console.log(`${backend}/${scene}: ${moving ? 'moving' : 'STATIC'} (${result.currentFps} FPS)`);
        if (process.env.ANIMATION_ASSERT === '1') {
          assert.ok(moving, `${backend}/${scene}: repeated identical animation frame`);
        }
      }
    }
    assert.deepEqual(errors, [], 'browser errors');
  } finally {
    await browser.close();
    if (process.env.ANIMATION_OUTPUT) {
      writeFileSync(process.env.ANIMATION_OUTPUT, JSON.stringify(results, null, 2) + '\n');
    }
  }
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
