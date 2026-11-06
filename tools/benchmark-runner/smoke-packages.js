#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

const repoRoot = path.resolve(__dirname, '../..');
const baseUrl = (process.env.BENCHMARK_URL || 'http://127.0.0.1:8061').replace(/\/+$/, '');
const browserPath = process.env.CHROMIUM_PATH || chromium.executablePath();
const packageIds = (process.env.BENCHMARK_PACKAGES ||
  'aot-webgl,aot-webgpu,jit-webgl,jit-webgpu')
    .split(',').map(value => value.trim()).filter(Boolean);

function sha256(buffer) {
  return crypto.createHash('sha256').update(buffer).digest('hex');
}

async function selectPackage(page, packageId, identity) {
  const {codegen: engineMode, backend} = identity;
  const engineButton = page.locator(`[data-engine-mode="${engineMode}"]`);
  if (await engineButton.getAttribute('aria-pressed') !== 'true') {
    await engineButton.click();
  }
  if (await page.locator('#backend').inputValue() !== backend) {
    await page.locator('#backend').selectOption(backend);
  }
  await page.locator('#package-source').selectOption(packageId);
  await page.waitForFunction(expectedPackage => {
    const frame = document.querySelector('#scene-frame');
    const identity = document.querySelector('#engine-identity').textContent;
    return frame.contentWindow?.location.href.includes(`/packages/${expectedPackage}/`) &&
      !identity.startsWith('正在读取') && !document.querySelector('#engine-identity')
        .classList.contains('identity-error');
  }, packageId);
}

async function runtimeEvidence(page) {
  return page.evaluate(() => {
    const view = document.querySelector('#scene-frame').contentWindow.__tgfxBenchmarkView;
    return view?.getAOTEvidence?.() ?? null;
  });
}

async function main() {
  const browser = await chromium.launch({
    headless: true,
    executablePath: browserPath,
    args: ['--no-sandbox', '--enable-unsafe-webgpu'],
  });
  const output = {
    schemaVersion: 'tgfx-benchmark.package-ui-smoke.v1',
    baseUrl,
    packages: {},
    executionStatus: 'completed',
  };
  try {
    for (const packageId of packageIds) {
      const identityPath = path.resolve(repoRoot, `../packages/${packageId}/tgfx-build.json`);
      const identity = JSON.parse(fs.readFileSync(identityPath, 'utf8'));
      const page = await browser.newPage({
        viewport: {width: 1280, height: 800},
        deviceScaleFactor: 1,
      });
      const errors = [];
      page.on('pageerror', error => errors.push(error.message));
      await page.goto(`${baseUrl}/benchmark-ui.html`, {waitUntil: 'load'});
      await selectPackage(page, packageId, identity);

      const displayedIdentity = await page.locator('#engine-identity').textContent();
      const identityTitle = await page.locator('#engine-identity').getAttribute('title');
      assert.ok(displayedIdentity.includes(identity.commit.slice(0, 8)),
          `${packageId}: UI does not show the compact TGFX commit`);
      assert.equal(identityTitle, identity.commit,
          `${packageId}: UI tooltip does not retain the full TGFX commit`);
      assert.ok(displayedIdentity.includes(identity.codegen === 'aot' ? 'AOT' : 'JIT'),
          `${packageId}: UI does not show the codegen mode`);
      assert.ok(displayedIdentity.includes(identity.backend === 'webgpu' ? 'WebGPU' : 'WebGL'),
          `${packageId}: UI does not show the backend`);

      await page.waitForFunction(() =>
        document.querySelector('#scene-frame').contentWindow.__tgfxBenchmark?.readyAt != null,
      null, {timeout: 30000});
      const frame = page.frameLocator('#scene-frame');
      const firstFrame = await frame.locator('canvas').screenshot();
      await page.waitForTimeout(500);
      const secondFrame = await frame.locator('canvas').screenshot();
      assert.notEqual(sha256(firstFrame), sha256(secondFrame),
          `${packageId}: preview canvas is not animating`);

      const evidence = await runtimeEvidence(page);
      assert.ok(evidence, `${packageId}: missing runtime AOT evidence`);
      if (identity.codegen === 'aot') {
        assert.equal(evidence.available, true, `${packageId}: AOT cache is unavailable`);
        assert.equal(evidence.isLoaded, true, `${packageId}: AOT bundle did not load`);
        const actionableFailures = Object.entries(evidence.fallbackCounts || {})
            .reduce((total, [reason, count]) =>
              reason === 'DeferredTexture' ? total : total + Number(count || 0), 0);
        assert.equal(actionableFailures, 0,
            `${packageId}: AOT runtime recorded failed lookups`);
      } else {
        assert.ok(!evidence.available || evidence.isLoaded === false,
            `${packageId}: JIT package unexpectedly loaded an AOT bundle`);
      }

      await page.locator('[data-run-mode="fixed"]').click();
      await page.locator('#count').fill('32');
      await page.locator('#measurement').fill('800');
      await page.locator('#run').click();
      await page.locator('#report-view').waitFor({state: 'visible', timeout: 30000});
      const report = await page.evaluate(() => ({
        subtitle: document.querySelector('#report-subtitle').textContent,
        subtitleTitle: document.querySelector('#report-subtitle').title,
        description: document.querySelector('#report-description').textContent,
        warning: document.querySelector('#report-warning').textContent,
        samples: Number(document.querySelector('#metric-samples').textContent),
      }));
      assert.ok(report.subtitle.includes(identity.commit.slice(0, 8)),
          `${packageId}: report subtitle does not show the compact TGFX commit`);
      assert.ok(report.subtitleTitle.includes(identity.commit),
          `${packageId}: report subtitle tooltip does not retain the full TGFX commit`);
      assert.ok(report.description.includes(identity.commit),
          `${packageId}: report details do not show the TGFX commit`);
      assert.ok(report.samples >= 2, `${packageId}: report retained too few samples`);
      assert.doesNotMatch(report.warning, /AOT 包没有成功加载|JIT\/main 包意外加载/,
          `${packageId}: report flags an AOT runtime mismatch`);

      await page.locator('[data-view="scene"]').click();
      await page.setViewportSize({width: 390, height: 844});
      await page.waitForFunction(() => {
        const stage = document.querySelector('#scene-stage');
        const canvas = document.querySelector('#scene-frame').contentWindow
          .document.querySelector('#benchmark');
        return canvas && Math.abs(canvas.clientWidth - stage.clientWidth) <= 1;
      });
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth),
          true, `${packageId}: narrow viewport overflows horizontally`);
      assert.deepEqual(errors, [], `${packageId}: browser errors`);

      output.packages[packageId] = {
        commit: identity.commit,
        codegen: identity.codegen,
        backend: identity.backend,
        runtimeAOT: evidence,
        reportSamples: report.samples,
        desktopWidth: 1280,
        mobileWidth: 390,
      };
      console.log(`${packageId}: switch, animation, AOT evidence, report and narrow layout passed`);
      await page.close();
    }
  } finally {
    await browser.close();
  }

  const outputPath = path.join(repoRoot, 'results', 'package-ui-smoke.json');
  fs.mkdirSync(path.dirname(outputPath), {recursive: true});
  fs.writeFileSync(outputPath, `${JSON.stringify(output, null, 2)}\n`, 'utf8');
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
