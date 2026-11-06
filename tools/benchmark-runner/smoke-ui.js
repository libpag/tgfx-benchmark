#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const {createHash} = require('node:crypto');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

const browserPath = process.env.CHROMIUM_PATH || chromium.executablePath();
const baseUrl = process.env.BENCHMARK_URL || 'http://127.0.0.1:8061/benchmark-ui.html';
const backends = (process.env.BENCHMARK_BACKENDS || 'webgl,webgpu')
    .split(',').map(value => value.trim()).filter(Boolean);
const engineModes = (process.env.BENCHMARK_ENGINE_MODES || 'aot,jit')
    .split(',').map(value => value.trim()).filter(Boolean);
const packages = engineModes.flatMap(engineMode =>
  backends.map(backend => ({engineMode, backend})));
const motionMarkScenes = [
  'motionmark.multiply',
  'motionmark.canvas-arcs',
  'motionmark.leaves',
  'motionmark.canvas-lines',
  'motionmark.images',
  'motionmark.design',
  'motionmark.suits',
];

async function canvasHash(frame) {
  return createHash('sha256').update(await frame.locator('canvas').screenshot()).digest('hex');
}

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

async function assertAnimated(page, backend, scene) {
  await page.locator('#scene').selectOption(scene);
  const frame = page.frameLocator('#scene-frame');
  await frame.locator('canvas').waitFor();
  await page.waitForFunction(
      () => document.querySelector('#scene-frame').contentWindow.__tgfxBenchmark?.readyAt != null);
  await page.waitForTimeout(800);
  const first = await canvasHash(frame);
  await page.waitForTimeout(800);
  assert.notEqual(first, await canvasHash(frame), `${backend}/${scene}: animation did not change`);
  console.log(`${backend}/${scene}: moving`);
}

async function assertCanvasAndMouse(page, backend) {
  await page.locator('#scene').selectOption('motionmark.multiply');
  await page.waitForFunction(() => {
    const win = document.querySelector('#scene-frame').contentWindow;
    return win.location.search.includes('case=motionmark.multiply') &&
      win.__tgfxBenchmark?.readyAt != null;
  });
  const frame = page.frameLocator('#scene-frame');
  const geometry = await page.evaluate(() => {
    const stage = document.querySelector('#scene-stage');
    const win = document.querySelector('#scene-frame').contentWindow;
    const canvas = win.document.querySelector('#benchmark');
    return {
      stageWidth: stage.clientWidth, stageHeight: stage.clientHeight,
      cssWidth: canvas.clientWidth, cssHeight: canvas.clientHeight,
      pixelWidth: canvas.width, pixelHeight: canvas.height, dpr: win.devicePixelRatio,
    };
  });
  assert.ok(Math.abs(geometry.stageWidth - geometry.cssWidth) <= 1 &&
    Math.abs(geometry.stageHeight - geometry.cssHeight) <= 1,
    `${backend}: canvas does not fill the scene`);
  assert.ok(Math.abs(geometry.pixelWidth - geometry.cssWidth * geometry.dpr) <= 1 &&
    Math.abs(geometry.pixelHeight - geometry.cssHeight * geometry.dpr) <= 1,
    `${backend}: canvas backing resolution does not match DPR`);
  for (const [xFraction, yFraction] of [[.08, .12], [.3, .45], [.87, .82]]) {
    const targetX = Math.round(geometry.cssWidth * xFraction);
    const targetY = Math.round(geometry.cssHeight * yFraction);
    await frame.locator('#benchmark').hover({position: {x: targetX, y: targetY}});
    await page.waitForFunction(([x, y]) => {
      const win = document.querySelector('#scene-frame').contentWindow;
      const stats = JSON.parse(win.__tgfxBenchmarkView.getBenchmarkLiveStatsJson());
      const canvas = win.document.querySelector('#benchmark');
      return Math.abs(stats.mouseX - x * canvas.width / canvas.clientWidth) < 4 &&
        Math.abs(stats.mouseY - y * canvas.height / canvas.clientHeight) < 4;
    }, [targetX, targetY], {timeout: 5000});
  }
  console.log(`${backend}: fill, DPR and mouse coordinates passed`);
}

async function assertActualSceneCount(page, backend, scene, count) {
  await page.locator('#scene').selectOption(scene);
  await page.locator('#count').fill(String(count));
  await page.locator('#count').dispatchEvent('change');
  await page.waitForFunction(([expectedScene, expectedCount]) => {
    const win = document.querySelector('#scene-frame').contentWindow;
    if (win.__tgfxBenchmark?.readyAt == null) return false;
    const stats = JSON.parse(win.__tgfxBenchmarkView.getBenchmarkLiveStatsJson());
    return win.location.search.includes(`case=${expectedScene}`) &&
      stats.sceneCount === expectedCount;
  }, [scene, count], {timeout: 20000});
  console.log(`${backend}/${scene}: actual count ${count}`);
}

async function assertSceneRenders(page, backend, scene) {
  await page.locator('#scene').selectOption(scene);
  await page.locator('#count').fill('300');
  await page.locator('#count').dispatchEvent('change');
  await page.waitForFunction((expectedScene) => {
    const win = document.querySelector('#scene-frame').contentWindow;
    return win.__tgfxBenchmark?.firstDrawAt != null &&
      win.location.search.includes(`case=${expectedScene}`);
  }, scene, {timeout: 30000});
  console.log(`${backend}/${scene}: rendered`);
}

async function main() {
  const browser = await chromium.launch({
    headless: true,
    executablePath: browserPath,
    args: ['--no-sandbox', '--enable-unsafe-webgpu'],
  });
  try {
    for (const {engineMode, backend} of packages) {
      const page = await browser.newPage({viewport: {width: 1440, height: 900}, deviceScaleFactor: 2});
      const errors = [];
      page.on('pageerror', error => errors.push(error.message));
      await page.goto(baseUrl);
      await selectPackage(page, engineMode, backend);
      assert.equal(await page.locator('#scene option[value="motionmark.paths"]').count(), 0);
      await page.locator('#scene').selectOption('motionmark.canvas-arcs');
      assert.equal(await page.locator('#count').inputValue(), '8');
      await page.locator('#scene').selectOption('motionmark.canvas-lines');
      assert.equal(await page.locator('#count').inputValue(), '16');
      await assertCanvasAndMouse(page, backend);
      await page.waitForFunction(
          () => document.querySelector('#render-resolution').textContent.includes('浏览器刷新上限'));
      await page.locator('#target-fps').fill('240');
      const unloaded = await page.evaluate(() =>
        document.querySelector('#scene-frame').contentWindow.__tgfxBenchmarkUnloadedDisplayFps?.());
      if (unloaded) {
        await page.locator('#run').click();
        await page.waitForFunction(
            () => document.querySelector('#error').textContent.includes('浏览器刷新上限'));
        assert.equal(await page.locator('#cancel').isVisible(), false,
            `${backend}: impossible target should be rejected before starting`);
      }
      await page.locator('#target-fps').fill('60');
      await assertActualSceneCount(page, backend, 'motionmark.multiply', 10000);
      await assertActualSceneCount(page, backend, 'motionmark.design', 128);
      for (const scene of motionMarkScenes) {
        await assertSceneRenders(page, backend, scene);
      }
      await page.locator('#scene').selectOption('motionmark.canvas-lines');
      await page.locator('#count').fill('12');
      await page.locator('#count').dispatchEvent('change');
      for (const scene of motionMarkScenes) {
        await assertAnimated(page, backend, scene);
      }
      await assertAnimated(page, backend, 'motionmark.canvas-lines');
      await page.locator('[data-run-mode="manual"]').click();
      await page.locator('#run').click();
      try {
        await page.waitForFunction(
            () => document.querySelector('#live-phase').textContent === '测量中' &&
              Number(document.querySelector('#live-samples').textContent) >= 2 &&
              !document.querySelector('#stop').disabled, null, {timeout: 25000});
      } catch (error) {
        const diagnostic = await page.evaluate(() => {
          const frame = document.querySelector('#scene-frame');
          return {
            phase: document.querySelector('#live-phase').textContent,
            samples: document.querySelector('#live-samples').textContent,
            disabled: document.querySelector('#stop').disabled,
            cancelVisible: !document.querySelector('#cancel').classList.contains('hidden'),
            error: document.querySelector('#error').textContent,
            frameUrl: frame.contentWindow?.location.href,
            frameReady: frame.contentWindow?.__tgfxBenchmark?.readyAt,
            frameStats: frame.contentWindow?.__tgfxBenchmarkView?.getBenchmarkLiveStatsJson(),
          };
        });
        throw new Error(`${backend}: manual run did not start: ${JSON.stringify(diagnostic)}`, {cause: error});
      }
      assert.equal(await page.locator('#report-view').isVisible(), false);
      await page.waitForTimeout(1300);
      assert.equal(await page.locator('#report-view').isVisible(), false);
      await page.locator('#stop').click();
      assert.equal(await page.locator('#report-view').isVisible(), true);
      let result = await page.evaluate(() => ({
        header: document.querySelector('#report-subtitle').textContent,
        samples: document.querySelector('#metric-samples').textContent,
        p95: document.querySelector('#metric-p95').textContent,
      }));
      assert.match(result.header, /手动观察/);
      assert.ok(Number(result.samples) >= 2);
      assert.notEqual(result.p95, '-');

      await page.locator('[data-view="scene"]').click();
      await page.waitForFunction(
          () => !document.querySelector('#scene-frame').contentWindow.__tgfxBenchmark?.benchmarkSummary);
      await page.locator('[data-run-mode="fixed"]').click();
      await page.locator('#measurement').fill('1200');
      await page.locator('#run').click();
      await page.locator('#report-view').waitFor({state: 'visible', timeout: 20000});
      result = await page.evaluate(() => ({
        header: document.querySelector('#report-subtitle').textContent,
        description: document.querySelector('#report-description').textContent,
      }));
      assert.match(result.header, /固定时长/);
      assert.match(result.description, /固定时长测量/);
      await page.locator('[data-view="scene"]').click();
      await page.waitForFunction(
          () => !document.querySelector('#scene-frame').contentWindow.__tgfxBenchmark?.benchmarkSummary);
      await page.locator('[data-test-intent="cold-start"]').click();
      assert.equal(await page.locator('#scene').inputValue(), 'coldstart.shader-mix');
      assert.equal(await page.locator('#warmup').count(), 0);
      await page.waitForFunction(() => {
        const win = document.querySelector('#scene-frame').contentWindow;
        return win?.location.search.includes('case=coldstart.shader-mix') &&
          win.__tgfxBenchmark?.firstDrawAt != null;
      });
      await page.locator('#count').fill('64');
      await page.locator('#run').click();
      await page.locator('#report-view').waitFor({state: 'visible', timeout: 30000});
      const cold = await page.evaluate(() => ({
        header: document.querySelector('#report-subtitle').textContent,
        coldVisible: !document.querySelector('#cold-report').classList.contains('hidden'),
        steadyHidden: document.querySelector('#steady-report').classList.contains('hidden'),
        firstVisible: document.querySelector('#cold-first-visible').textContent,
        intent: document.querySelector('#scene-frame').contentWindow
          .__tgfxBenchmark.benchmarkSummary.testIntent,
      }));
      assert.match(cold.header, /首帧测试/);
      assert.equal(cold.coldVisible, true);
      assert.equal(cold.steadyHidden, true);
      assert.notEqual(cold.firstVisible, '-');
      assert.equal(cold.intent, 'cold-start');
      await page.locator('[data-view="scene"]').click();
      await page.locator('[data-test-intent="steady-state"]').click();
      await page.locator('#scene').selectOption('motionmark.multiply');
      await page.locator('[data-run-mode="ramp"]').click();
      assert.equal(await page.locator('#count').getAttribute('max'), null,
          `${backend}: adaptive ramp must not expose an object maximum`);
      assert.equal(await page.locator('#ramp-step').count(), 0);
      assert.equal(await page.locator('#ramp-max').count(), 0);
      await page.locator('#count').fill('5000');
      await page.locator('#target-fps').fill('55');
      await page.locator('#run').click();
      await page.waitForFunction(
          () => Number(document.querySelector('#live-count').textContent) > 5000,
          null, {timeout: 30000});
      try {
        await page.waitForFunction(
            () => ['估算触边', '稳定观察中'].includes(
              document.querySelector('#scene-phase').textContent),
            null, {timeout: 60000});
      } catch (error) {
        const diagnostic = await page.evaluate(() => {
          const win = document.querySelector('#scene-frame').contentWindow;
          return {
            phase: document.querySelector('#scene-phase').textContent,
            count: document.querySelector('#live-count').textContent,
            peak: document.querySelector('#live-peak').textContent,
            settled: document.querySelector('#live-settled').textContent,
            stats: win.__tgfxBenchmarkView.getBenchmarkLiveStatsJson(),
          };
        });
        throw new Error(`${backend}: adaptive ramp did not settle: ${JSON.stringify(diagnostic)}`,
            {cause: error});
      }
      await page.waitForFunction(
          () => !document.querySelector('#stop').disabled,
          null, {timeout: 10000});
      const adaptiveLive = await page.evaluate(() => ({
        count: Number(document.querySelector('#live-count').textContent),
        peak: Number(document.querySelector('#live-peak').textContent),
        settled: Number(document.querySelector('#live-settled').textContent),
      }));
      assert.ok(adaptiveLive.peak >= adaptiveLive.settled);
      assert.ok(adaptiveLive.settled > 5000);
      assert.equal(adaptiveLive.count, adaptiveLive.settled);
      assert.equal(await page.locator('#report-view').isVisible(), false);
      const steadyFrame = await canvasHash(page.frameLocator('#scene-frame'));
      await page.waitForTimeout(750);
      assert.notEqual(steadyFrame, await canvasHash(page.frameLocator('#scene-frame')),
          `${backend}: steady animation stopped`);
      await page.waitForFunction(() => !document.querySelector('#stop').disabled,
          null, {timeout: 30000});
      await page.locator('#stop').click();
      assert.equal(await page.locator('#report-view').isVisible(), true);
      const ramp = await page.evaluate(() => ({
        header: document.querySelector('#report-subtitle').textContent,
        initial: document.querySelector('#ramp-initial').textContent,
        peak: document.querySelector('#ramp-peak').textContent,
        settled: document.querySelector('#ramp-settled').textContent,
        target: document.querySelector('#ramp-target').textContent,
        retractions: Number(document.querySelector('#ramp-retractions').textContent),
        verdict: document.querySelector('#ramp-verdict').textContent,
        samples: Number(document.querySelector('#metric-samples').textContent),
        p95: document.querySelector('#metric-p95').textContent,
        p99: document.querySelector('#metric-p99').textContent,
        note: document.querySelector('#interval-note').textContent,
        targetReached: document.querySelector('#scene-frame').contentWindow
          .__tgfxBenchmark.benchmarkSummary.adaptiveTargetReached,
        thresholdReached: document.querySelector('#scene-frame').contentWindow
          .__tgfxBenchmark.benchmarkSummary.adaptiveThresholdReached,
      }));
      assert.match(ramp.header, /快速爬坡|自适应爬坡/);
      assert.equal(ramp.initial, '5000');
      assert.ok(Number(ramp.peak) >= Number(ramp.settled));
      assert.equal(ramp.target, '≥ 54.5 FPS');
      if (ramp.targetReached) {
        assert.ok(Number(ramp.settled) > 0);
        assert.match(ramp.verdict, /没有设置对象数上限/);
        assert.ok(ramp.samples >= 2);
        assert.notEqual(ramp.p99, '-');
        assert.ok(Number.parseFloat(ramp.p95) <= (1000 / 55) * 1.05 + 1,
            `${backend}: stable p95 exceeded target budget: ${ramp.p95}`);
      } else {
        assert.equal(ramp.thresholdReached, true);
        assert.ok(Number(ramp.settled) > 5000);
        assert.match(ramp.verdict, /估算容量/);
        assert.ok(ramp.samples >= 2);
        assert.notEqual(ramp.p95, '-');
        assert.notEqual(ramp.p99, '-');
      }
      assert.match(ramp.note, /DPR/);
      assert.match(ramp.note, /渲染像素/);
      await page.waitForFunction(() => {
        const chart = document.querySelector('#ramp-chart');
        if (!chart.width || !chart.height) return false;
        const pixels = chart.getContext('2d').getImageData(0, 0, chart.width, chart.height).data;
        for (let i = 3; i < pixels.length; i += 4) {
          if (pixels[i] > 0) return true;
        }
        return false;
      });
      await page.setViewportSize({width: 390, height: 844});
      await page.locator('[data-view="scene"]').click();
      await page.waitForFunction(() => {
        const stage = document.querySelector('#scene-stage');
        const win = document.querySelector('#scene-frame').contentWindow;
        const canvas = win?.document.querySelector('#benchmark');
        return canvas && Math.abs(canvas.clientWidth - stage.clientWidth) <= 1 &&
          Math.abs(canvas.width - stage.clientWidth * devicePixelRatio) <= 1;
      });
      assert.equal(await page.locator('#scene-view').isVisible(), true);
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true,
          `${backend}: narrow viewport overflows horizontally`);
      await page.locator('[data-run-mode="manual"]').click();
      await page.locator('#run').click();
      await page.waitForFunction(() => !document.querySelector('#cancel').classList.contains('hidden'),
          null, {timeout: 20000});
      await page.setViewportSize({width: 480, height: 844});
      await page.waitForFunction(() => document.querySelector('#error').textContent.includes('尺寸发生变化'),
          null, {timeout: 10000});
      assert.equal(await page.locator('#cancel').isVisible(), false,
          `${backend}: resize should invalidate the active measurement`);
      assert.deepEqual(errors, [], `${backend}: browser errors`);
      console.log(
          `${engineMode}/${backend}: manual, fixed, ramp, particle animation, narrow layout passed`);
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
