#!/usr/bin/env node
'use strict';

const crypto = require('node:crypto');
const fs = require('fs');
const path = require('path');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const repoRoot = path.resolve(__dirname, '../..');

const executablePath = process.env.CHROMIUM_PATH || chromium.executablePath();

function parseArgs(argv) {
  const args = {
    backend: 'webgl',
    engineMode: 'aot',
    packageId: null,
    baseUrl: 'http://localhost:8061',
    caseId: 'motionmark.multiply',
    count: 512,
    testIntent: 'steady-state',
    initialWindowFrames: 30,
    measurementMs: 1000,
    recorder: true,
    runtimeFallback: false,
    runtimeProbe: 'full',
    webgpuCoalesceUploads: null,
    webgpuDynamicBindGroups: null,
    compactRRectInstances: null,
    adaptive: false,
    targetFps: 60,
    stableWindowMs: 3000,
    adaptiveDecisionTimeoutMs: 60000,
    output: null,
    budgetMs: 17.0,
  };
  for (let i = 2; i < argv.length; ++i) {
    const argument = argv[i];
    const separator = argument.indexOf('=');
    const key = separator > 0 ? argument.slice(0, separator) : argument;
    const value = separator > 0 ? argument.slice(separator + 1) : argv[++i];
    if (value === undefined || value.startsWith('--')) {
      throw new Error(`Missing value for ${key}`);
    }
    switch (key) {
      case '--backend': args.backend = value; break;
      case '--engine': args.engineMode = value; break;
      case '--package': args.packageId = value; break;
      case '--base-url': args.baseUrl = value.replace(/\/+$/, ''); break;
      case '--case': args.caseId = value; break;
      case '--count': args.count = Number(value); break;
      case '--test-intent': args.testIntent = value; break;
      case '--initial-window': args.initialWindowFrames = Number(value); break;
      case '--measurement': args.measurementMs = Number(value); break;
      case '--recorder': args.recorder = value !== '0'; break;
      case '--runtime-fallback': args.runtimeFallback = value !== '0'; break;
      case '--runtime-probe': args.runtimeProbe = value; break;
      case '--webgpu-coalesce-uploads':
        args.webgpuCoalesceUploads = value !== '0';
        break;
      case '--webgpu-dynamic-bind-groups':
        args.webgpuDynamicBindGroups = value !== '0';
        break;
      case '--compact-rrect-instances':
        args.compactRRectInstances = value !== '0';
        break;
      case '--adaptive': args.adaptive = value !== '0'; break;
      case '--target-fps': args.targetFps = Number(value); break;
      case '--stable-window': args.stableWindowMs = Number(value); break;
      case '--adaptive-timeout': args.adaptiveDecisionTimeoutMs = Number(value); break;
      case '--output': args.output = value; break;
      case '--budget': args.budgetMs = Number(value); break;
      default: throw new Error(`Unknown argument: ${key}`);
    }
  }
  if (!['aot', 'jit'].includes(args.engineMode)) {
    throw new Error(`Unsupported engine mode: ${args.engineMode}`);
  }
  if (!['webgl', 'webgpu'].includes(args.backend)) {
    throw new Error(`Unsupported backend: ${args.backend}`);
  }
  if (args.packageId === null) {
    args.packageId = `${args.engineMode}-${args.backend}`;
  } else {
    const manifestPath = path.resolve(
        repoRoot, '../packages', args.packageId, 'tgfx-build.json');
    if (!fs.existsSync(manifestPath)) {
      throw new Error(`Package manifest is missing: ${manifestPath}`);
    }
    const identity = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    args.engineMode = identity.codegen;
    args.backend = identity.backend;
  }
  if (!['cold-start', 'steady-state'].includes(args.testIntent)) {
    throw new Error(`Unsupported test intent: ${args.testIntent}`);
  }
  if (!['full', 'identity'].includes(args.runtimeProbe)) {
    throw new Error(`Unsupported runtime probe: ${args.runtimeProbe}`);
  }
  for (const name of [
    'count',
    'initialWindowFrames',
    'measurementMs',
    'targetFps',
    'stableWindowMs',
    'adaptiveDecisionTimeoutMs',
  ]) {
    if (!Number.isFinite(args[name]) || args[name] < 1) {
      throw new Error(`Invalid ${name}: ${args[name]}`);
    }
  }
  if (args.adaptive && args.testIntent !== 'steady-state') {
    throw new Error('Adaptive mode requires steady-state test intent');
  }
  return args;
}

function quantile(values, fraction) {
  if (values.length === 0) {
    return null;
  }
  const sorted = [...values].sort((a, b) => a - b);
  const index = Math.min(sorted.length - 1, Math.max(0, Math.ceil(sorted.length * fraction) - 1));
  return sorted[index];
}

function actionableAOTFailureCount(evidence) {
  return Object.entries(evidence?.fallbackCounts || {}).reduce((total, [reason, count]) =>
    reason === 'DeferredTexture' ? total : total + Number(count || 0), 0);
}

function installRuntimeProbe(options) {
  const expectedBackend = options.expectedBackend;
  const mode = options.mode;
  const probe = {
    expectedBackend,
    mode,
    webgl: {
      contexts: 0,
      drawCalls: 0,
      renderer: null,
      vendor: null,
    },
    webgpu: {
      requestAdapterCalls: 0,
      canvasConfigures: 0,
      renderPasses: 0,
      drawCalls: 0,
      pipelines: 0,
      bindGroups: 0,
      writeBufferCalls: 0,
      writeBufferBytes: 0,
      vertexBuffers: 0,
      adapterInfo: null,
    },
  };
  globalThis.__tgfxRuntimeProbe = probe;

  const wrapPrototypeMethod = (prototype, name, callback) => {
    if (!prototype || typeof prototype[name] !== 'function') {
      return;
    }
    const original = prototype[name];
    prototype[name] = function(...args) {
      const result = original.apply(this, args);
      callback(this, args, result);
      return result;
    };
  };

  if (globalThis.navigator?.gpu && typeof globalThis.navigator.gpu.requestAdapter === 'function') {
    const originalRequestAdapter = globalThis.navigator.gpu.requestAdapter.bind(globalThis.navigator.gpu);
    globalThis.navigator.gpu.requestAdapter = async (...args) => {
      probe.webgpu.requestAdapterCalls++;
      const adapter = await originalRequestAdapter(...args);
      if (adapter?.info) {
        probe.webgpu.adapterInfo = {
          vendor: adapter.info.vendor || '',
          architecture: adapter.info.architecture || '',
          device: adapter.info.device || '',
          description: adapter.info.description || '',
        };
      } else if (adapter) {
        probe.webgpu.adapterInfo = {};
      }
      return adapter;
    };
  }

  wrapPrototypeMethod(globalThis.GPUCanvasContext?.prototype, 'configure', () => {
    probe.webgpu.canvasConfigures++;
  });
  if (mode === 'full') {
    wrapPrototypeMethod(globalThis.GPUCommandEncoder?.prototype, 'beginRenderPass', () => {
      probe.webgpu.renderPasses++;
    });
    for (const name of ['draw', 'drawIndexed']) {
      wrapPrototypeMethod(globalThis.GPURenderPassEncoder?.prototype, name, () => {
        probe.webgpu.drawCalls++;
      });
    }
    wrapPrototypeMethod(globalThis.GPURenderPassEncoder?.prototype, 'setPipeline', () => {
      probe.webgpu.pipelines++;
    });
    wrapPrototypeMethod(globalThis.GPURenderPassEncoder?.prototype, 'setBindGroup', () => {
      probe.webgpu.bindGroups++;
    });
    if (globalThis.GPUQueue?.prototype?.writeBuffer) {
      const originalWriteBuffer = globalThis.GPUQueue.prototype.writeBuffer;
      globalThis.GPUQueue.prototype.writeBuffer = function(buffer, bufferOffset, data, dataOffset,
        size) {
        const result = originalWriteBuffer.call(this, buffer, bufferOffset, data, dataOffset, size);
        probe.webgpu.writeBufferCalls++;
        probe.webgpu.writeBufferBytes += Number.isFinite(size)
          ? size
          : Math.max(0, data.byteLength - (dataOffset || 0));
        return result;
      };
    }
    wrapPrototypeMethod(globalThis.GPURenderPassEncoder?.prototype, 'setVertexBuffer', () => {
      probe.webgpu.vertexBuffers++;
    });
  }

  for (const prototype of [
    globalThis.WebGLRenderingContext?.prototype,
    globalThis.WebGL2RenderingContext?.prototype,
  ]) {
    if (!prototype) {
      continue;
    }
    if (mode === 'full') {
      for (const name of [
        'drawArrays',
        'drawElements',
        'drawArraysInstanced',
        'drawElementsInstanced',
        'drawRangeElements',
      ]) {
        wrapPrototypeMethod(prototype, name, () => {
          probe.webgl.drawCalls++;
        });
      }
    }
  }

  if (globalThis.HTMLCanvasElement?.prototype?.getContext) {
    const originalGetContext = globalThis.HTMLCanvasElement.prototype.getContext;
    globalThis.HTMLCanvasElement.prototype.getContext = function(type, ...args) {
      const context = originalGetContext.call(this, type, ...args);
      const normalized = String(type).toLowerCase();
      if (context && (normalized === 'webgl' || normalized === 'webgl2' ||
          normalized === 'experimental-webgl')) {
        probe.webgl.contexts++;
        try {
          const info = context.getExtension('WEBGL_debug_renderer_info');
          if (info) {
            probe.webgl.renderer = context.getParameter(info.UNMASKED_RENDERER_WEBGL);
            probe.webgl.vendor = context.getParameter(info.UNMASKED_VENDOR_WEBGL);
          }
        } catch (error) {
          probe.webgl.debugInfoError = error.message;
        }
      }
      return context;
    };
  }
}

function pageUrl(args, count) {
  const base = `${args.baseUrl}/packages/${args.packageId}/index.html`;
  const params = new URLSearchParams({
    benchmark: '1',
    case: args.caseId,
    count: String(count),
    width: '900',
    height: '600',
    recorder: args.recorder ? '1' : '0',
    backend: args.backend,
    aot: args.engineMode === 'aot' && !args.runtimeFallback ? '1' : '0',
    testIntent: args.testIntent,
    runNonce: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
  });
  if (args.testIntent === 'cold-start') {
    params.set('initialWindowFrames', String(args.initialWindowFrames));
  } else if (args.adaptive) {
    params.set('adaptive', '1');
    params.set('targetFps', String(args.targetFps));
    params.set('stableMs', String(args.stableWindowMs));
  } else {
    params.set('measurementMs', String(args.measurementMs));
  }
  if (args.webgpuCoalesceUploads !== null) {
    params.set('webgpuCoalesceUploads', args.webgpuCoalesceUploads ? '1' : '0');
  }
  if (args.webgpuDynamicBindGroups !== null) {
    params.set('webgpuDynamicBindGroups', args.webgpuDynamicBindGroups ? '1' : '0');
  }
  if (args.compactRRectInstances !== null) {
    params.set('compactRRectInstances', args.compactRRectInstances ? '1' : '0');
  }
  return `${base}?${params.toString()}`;
}

function sha256(buffer) {
  return crypto.createHash('sha256').update(buffer).digest('hex');
}

async function hashServedArtifact(baseUrl, packageId, artifact) {
  if (!artifact?.path) {
    return null;
  }
  const url = `${baseUrl}/packages/${packageId}/${artifact.path}`;
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to fetch served artifact ${url}: HTTP ${response.status}`);
  }
  const bytes = Buffer.from(await response.arrayBuffer());
  const actualSha256 = sha256(bytes);
  return {
    url,
    path: artifact.path,
    bytes: bytes.length,
    sha256: actualSha256,
    manifestSha256: artifact.sha256 || null,
    matchesManifest: artifact.sha256 ? actualSha256 === artifact.sha256 : null,
  };
}

async function collectServedIdentity(args, packageIdentity) {
  const artifacts = packageIdentity.artifacts || {};
  const entry = await hashServedArtifact(args.baseUrl, args.packageId, artifacts.entry);
  const glue = await hashServedArtifact(args.baseUrl, args.packageId, artifacts.glue);
  const wasm = await hashServedArtifact(args.baseUrl, args.packageId, artifacts.wasm);
  return {entry, glue, wasm};
}

function phaseSummary(samples) {
  const values = (field) => samples.map(sample => Number(sample[field])).filter(Number.isFinite);
  const phase = field => {
    const input = values(field);
    return {
      unit: 'us',
      count: input.length,
      p50: quantile(input, 0.5),
      p95: quantile(input, 0.95),
      max: input.length > 0 ? Math.max(...input) : null,
    };
  };
  const intervalsMs = [];
  for (let index = 1; index < samples.length; ++index) {
    intervalsMs.push(
        (samples[index].callbackBeginUs - samples[index - 1].callbackBeginUs) / 1000);
  }
  return {
    units: {
      callbackInterval: 'ms (callbackBeginUs delta divided by 1000)',
      callbackCpu: 'us (callbackEndUs - callbackBeginUs)',
      sceneDraw: 'us',
      flush: 'us',
      submit: 'us',
    },
    callbackIntervalMs: {
      count: intervalsMs.length,
      p50: quantile(intervalsMs, 0.5),
      p95: quantile(intervalsMs, 0.95),
      p99: quantile(intervalsMs, 0.99),
      max: intervalsMs.length > 0 ? Math.max(...intervalsMs) : null,
      over20Ms: intervalsMs.filter(value => value > 20).length,
    },
    callbackCpuUs: (() => {
      const input = samples.map(sample => sample.callbackEndUs - sample.callbackBeginUs)
          .filter(Number.isFinite);
      return {
        unit: 'us',
        count: input.length,
        p50: quantile(input, 0.5),
        p95: quantile(input, 0.95),
        max: input.length > 0 ? Math.max(...input) : null,
      };
    })(),
    sceneDrawUs: phase('sceneDrawUs'),
    flushUs: phase('flushUs'),
    submitUs: phase('submitUs'),
    intervalSamplesMs: intervalsMs,
  };
}

function errorSummary(errors) {
  return errors.length === 0;
}

async function runOnce(browser, args, count) {
  const page = await browser.newPage({
    viewport: {width: 1000, height: 700},
    deviceScaleFactor: 1,
  });
  const pageErrors = [];
  const consoleMessages = [];
  page.on('pageerror', error => pageErrors.push({
    name: error.name,
    message: error.message,
    stack: error.stack || null,
  }));
  page.on('console', message => {
    if (message.type() === 'error' || message.type() === 'warning') {
      consoleMessages.push({type: message.type(), text: message.text()});
    }
  });
  await page.addInitScript(installRuntimeProbe, {
    expectedBackend: args.backend,
    mode: args.runtimeProbe,
  });
  const url = pageUrl(args, count);
  await page.goto(url, {waitUntil: 'domcontentloaded', timeout: 120000});
  let adaptiveDecisionReached = null;
  if (args.adaptive) {
    try {
      await page.waitForFunction(() => {
        const view = window.__tgfxBenchmarkView;
        if (!view?.getBenchmarkLiveStatsJson) {
          return false;
        }
        const stats = JSON.parse(view.getBenchmarkLiveStatsJson());
        return stats.adaptiveThresholdReached === true ||
          stats.adaptiveTargetReached === true ||
          stats.adaptiveCapacityLimited === true;
      }, null, {timeout: args.adaptiveDecisionTimeoutMs});
      adaptiveDecisionReached = true;
    } catch (error) {
      adaptiveDecisionReached = false;
    }
    await page.evaluate(() => window.__tgfxBenchmarkView?.stopBenchmarkRun?.());
    await page.waitForFunction(
        () => window.__tgfxBenchmark?.benchmarkSummary !== null,
        null,
        {timeout: 30000});
  } else {
    await page.waitForFunction(
        () => window.__tgfxBenchmark && window.__tgfxBenchmark.benchmarkSummary !== null,
        null,
        {timeout: Math.max(120000, args.measurementMs + 120000)});
  }
  const summary = await page.evaluate(() => window.__tgfxBenchmark.benchmarkSummary);
  const state = await page.evaluate(() => ({
    userAgent: navigator.userAgent,
    devicePixelRatio: window.devicePixelRatio,
    innerWidth: window.innerWidth,
    innerHeight: window.innerHeight,
    runtimeProbe: globalThis.__tgfxRuntimeProbe || null,
    benchmarkConfig: window.__tgfxBenchmarkConfig || null,
    readyAt: window.__tgfxBenchmark.readyAt,
    completeAt: window.__tgfxBenchmark.benchmarkCompleteAt,
    runtimeAOT: window.__tgfxBenchmarkView?.getAOTEvidence?.() ?? null,
    canvas: (() => {
      const canvas = document.getElementById('benchmark');
      if (!canvas) {
        return null;
      }
      const bounds = canvas.getBoundingClientRect();
      return {
        width: canvas.width,
        height: canvas.height,
        cssWidth: bounds.width,
        cssHeight: bounds.height,
      };
    })(),
  }));
  const intervals = [];
  for (let i = 1; i < summary.samples.length; ++i) {
    intervals.push((summary.samples[i].callbackBeginUs - summary.samples[i - 1].callbackBeginUs) / 1000);
  }
  const samples = Array.isArray(summary.samples) ? summary.samples : [];
  const actualCounts = [...new Set(samples.map(sample => sample.sceneCount))];
  const runtimeProbe = state.runtimeProbe;
  const actualBackendValid = args.backend === 'webgpu'
      ? args.runtimeProbe === 'identity'
        ? Boolean(runtimeProbe?.webgpu?.adapterInfo &&
            runtimeProbe.webgpu.canvasConfigures > 0 &&
            runtimeProbe.webgl?.contexts === 0)
        : Boolean(runtimeProbe?.webgpu?.adapterInfo &&
            runtimeProbe.webgpu.canvasConfigures > 0 &&
            runtimeProbe.webgpu.renderPasses > 0 &&
            runtimeProbe.webgpu.drawCalls > 0)
      : args.runtimeProbe === 'identity'
        ? Boolean(runtimeProbe?.webgl?.contexts > 0 &&
            runtimeProbe.webgpu?.canvasConfigures === 0)
        : Boolean(runtimeProbe?.webgl?.contexts > 0 && runtimeProbe.webgl.drawCalls > 0);
  const result = {
    backend: args.backend,
    packageCodegen: args.engineMode,
    requestedRuntimeMode: args.engineMode === 'aot' && !args.runtimeFallback ? 'aot' :
      'runtime-fallback',
    tgfxPackage: args.packageId,
    testIntent: summary.testIntent,
    caseId: args.caseId,
    sceneVersion: state.benchmarkConfig?.sceneVersion ?? null,
    count,
    recorder: args.recorder,
    runtimeProbeMode: args.runtimeProbe,
    webgpuDynamicBindGroups: args.webgpuDynamicBindGroups,
    compactRRectInstances: args.compactRRectInstances,
    executionStatus: summary.executionStatus,
    sampleCount: summary.sampleCount,
    recordedFrameCount: summary.recordedFrameCount,
    drawnFrameCount: summary.drawnFrameCount,
    overflowed: summary.overflowed,
    finalSceneCount: summary.finalSceneCount,
    adaptive: summary.adaptive,
    adaptiveDecisionReached,
    adaptiveSettledCount: summary.adaptiveSettledCount,
    adaptiveLowPassingCount: summary.adaptiveLowPassingCount,
    adaptiveHighFailingCount: summary.adaptiveHighFailingCount,
    adaptiveSearchIteration: summary.adaptiveSearchIteration,
    actualCounts,
    firstSubmitUs: summary.firstSubmitUs,
    drainAfterMeasurementUs: summary.drainAfterMeasurementUs,
    readyToCompleteMs: state.completeAt !== null && state.readyAt !== null
        ? state.completeAt - state.readyAt
        : null,
    runtimeAOT: state.runtimeAOT,
    environment: {
      browserVersion: browser.version(),
      chromiumExecutable: executablePath,
      userAgent: state.userAgent,
      devicePixelRatio: state.devicePixelRatio,
      pageViewport: {width: state.innerWidth, height: state.innerHeight},
      canvas: state.canvas,
      benchmarkConfig: state.benchmarkConfig,
      runtimeProbe,
      runtimeProbeDefinitions: {
        webgpuWriteBufferCalls:
          'Count of GPUQueue.writeBuffer calls from page load through result capture.',
        webgpuWriteBufferBytes:
          'Sum of explicit writeBuffer size arguments, or source byte length minus data offset, in bytes.',
      },
      node: process.versions,
    },
    coldStart: summary.testIntent === 'cold-start' ? {
      firstFrame: summary.firstFrame || null,
      firstVisibleUs: summary.coldStartFirstVisibleUs,
      initialWindowFrames: summary.coldStartFramesAfterVisible,
    } : null,
    intervalP50Ms: quantile(intervals, 0.5),
    intervalP95Ms: quantile(intervals, 0.95),
    intervalMaxMs: intervals.length > 0 ? Math.max(...intervals) : null,
    phases: phaseSummary(samples),
    summary,
    pageErrors,
    consoleMessages,
    actualBackendValid,
  };
  const expectedAOT = args.engineMode === 'aot' && !args.runtimeFallback;
  const aotValid = expectedAOT
      ? state.runtimeAOT?.available === true && state.runtimeAOT?.isLoaded === true &&
        actionableAOTFailureCount(state.runtimeAOT) === 0
      : state.runtimeAOT?.isLoaded !== true;
  result.aotValidity = aotValid;
  result.runtimeModeValidity = expectedAOT
      ? state.runtimeAOT?.isLoaded === true
      : state.benchmarkConfig?.aotRequested === false && state.runtimeAOT?.isLoaded !== true;
  result.pass = args.testIntent === 'cold-start'
      ? result.executionStatus === 'completed' &&
        result.sampleCount > 0 &&
        result.coldStart?.initialWindowFrames >= args.initialWindowFrames &&
        aotValid
      : args.recorder
      ? result.executionStatus === 'completed' &&
        !result.overflowed &&
        result.sampleCount >= 30 &&
        result.intervalP95Ms !== null &&
        result.intervalP95Ms <= args.budgetMs &&
        aotValid
      : result.executionStatus === 'completed' && !result.overflowed && aotValid;
  const consoleErrorCount = consoleMessages.filter(message => message.type === 'error').length;
  result.contractValid =
      result.executionStatus === 'completed' &&
      result.sampleCount > 0 &&
      result.drawnFrameCount > 0 &&
      (args.adaptive
        ? result.adaptive === true && result.adaptiveDecisionReached === true &&
          result.finalSceneCount > 0
        : result.actualCounts.length === 1 && result.actualCounts[0] === count) &&
      actualBackendValid &&
      result.runtimeModeValidity &&
      errorSummary(pageErrors) &&
      consoleErrorCount === 0;
  await page.close();
  return result;
}

async function main() {
  const args = parseArgs(process.argv);
  const browser = await chromium.launch({
    headless: true,
    executablePath,
    args: [
      '--no-sandbox',
      '--disable-dev-shm-usage',
      '--enable-unsafe-webgpu',
      '--disable-gpu-shader-disk-cache',
      '--disk-cache-size=1',
    ],
  });
  try {
    const manifestPath = path.resolve(
        __dirname, '../../..', 'packages', args.packageId, 'tgfx-build.json');
    const packageIdentity = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    const run = await runOnce(browser, args, args.count);
    const servedIdentity = await collectServedIdentity(args, packageIdentity);
    const output = {
      schemaVersion: 'tgfx-benchmark.web-case.v1',
      args,
      packageIdentity,
      servedIdentity,
      isolationLevel: args.testIntent === 'cold-start' ? 'fresh-process' : 'context+assets',
      runs: [run],
    };
    const json = JSON.stringify(output, null, 2) + '\n';
    if (args.output !== null) {
      fs.mkdirSync(path.dirname(path.resolve(args.output)), {recursive: true});
      fs.writeFileSync(args.output, json, 'utf8');
    }
    console.log(json);
  } finally {
    await browser.close();
  }
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
