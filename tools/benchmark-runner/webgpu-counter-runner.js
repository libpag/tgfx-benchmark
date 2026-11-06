#!/usr/bin/env node
'use strict';

const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const repoRoot = path.resolve(__dirname, '../..');

function loadPlaywright() {
  return require(process.env.PLAYWRIGHT_MODULE || 'playwright');
}

function defaultArgs() {
  return {
    baseUrl: process.env.BENCHMARK_URL || 'http://127.0.0.1:8061',
    packageId: 'aot-webgpu',
    caseId: 'motionmark.multiply',
    count: 512,
    testIntent: 'steady-state',
    initialWindowFrames: 30,
    measurementMs: 1000,
    webgpuCoalesceUploads: null,
    webgpuDynamicBindGroups: null,
    compactRRectInstances: null,
    output: null,
  };
}

function parseArgs(argv) {
  const args = defaultArgs();
  for (let index = 2; index < argv.length; ++index) {
    const argument = argv[index];
    const separator = argument.indexOf('=');
    const key = separator > 0 ? argument.slice(0, separator) : argument;
    const value = separator > 0 ? argument.slice(separator + 1) : argv[++index];
    if (value === undefined || value.startsWith('--')) {
      throw new Error(`Missing value for ${key}`);
    }
    switch (key) {
      case '--package': args.packageId = value; break;
      case '--case': args.caseId = value; break;
      case '--count': args.count = Number(value); break;
      case '--test-intent': args.testIntent = value; break;
      case '--initial-window': args.initialWindowFrames = Number(value); break;
      case '--measurement': args.measurementMs = Number(value); break;
      case '--webgpu-coalesce-uploads':
        args.webgpuCoalesceUploads = value !== '0';
        break;
      case '--webgpu-dynamic-bind-groups':
        args.webgpuDynamicBindGroups = value !== '0';
        break;
      case '--compact-rrect-instances':
        args.compactRRectInstances = value !== '0';
        break;
      case '--base-url': args.baseUrl = value; break;
      case '--output': args.output = value; break;
      default: throw new Error(`Unknown argument: ${key}`);
    }
  }
  return normalizeArgs(args);
}

function normalizeArgs(input) {
  const args = {...defaultArgs(), ...input};
  args.baseUrl = args.baseUrl.replace(/\/+$/, '');
  const manifestPath = path.resolve(
      repoRoot, '../packages', args.packageId, 'tgfx-build.json');
  if (!fs.existsSync(manifestPath)) {
    throw new Error(`Package manifest is missing: ${manifestPath}`);
  }
  const identity = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  if (identity.backend !== 'webgpu') {
    throw new Error(`GPU counter runner currently supports WebGPU packages only: ${args.packageId}`);
  }
  if (!['cold-start', 'steady-state'].includes(args.testIntent)) {
    throw new Error(`Invalid --test-intent: ${args.testIntent}`);
  }
  for (const name of ['count', 'initialWindowFrames', 'measurementMs']) {
    if (!Number.isFinite(args[name]) || args[name] < (name === 'count' ? 1 : 0)) {
      throw new Error(`Invalid --${name}: ${args[name]}`);
    }
  }
  return args;
}

function installWebGPUCounterProbe() {
  if (globalThis.__tgfxGPUCounterProbe || !globalThis.GPUCommandEncoder) {
    return;
  }

  const zeroCounters = () => ({
    commandBuffers: 0,
    renderPasses: 0,
    computePasses: 0,
    drawCalls: 0,
    indexedDrawCalls: 0,
    instancesDrawn: 0,
    verticesDrawn: 0,
    indicesDrawn: 0,
    pipelineBinds: 0,
    uniquePipelines: 0,
    bindGroupBinds: 0,
    vertexBufferBinds: 0,
    indexBufferBinds: 0,
    uploadOperations: 0,
    uploadedBytes: 0,
    uploadedPayloadBytes: 0,
    copyOperations: 0,
    copiedBytes: 0,
    commandBufferSubmits: 0,
    bufferCreates: 0,
    textureCreates: 0,
    renderPipelineCreates: 0,
    computePipelineCreates: 0,
    bindGroupCreates: 0,
    shaderModuleCreates: 0,
    canvasTextureAcquisitions: 0,
    logicalResourceBytesCreated: 0,
    logicalResourceBytesReleased: 0,
  });

  const state = {
    createdAt: performance.now(),
    records: [],
    totals: zeroCounters(),
    previousGlobal: zeroCounters(),
    liveResourceBytes: 0,
    peakLiveResourceBytes: 0,
    unknownTextureSizeCount: 0,
    sequence: 0,
    adapterInfo: null,
    staging: {
      pending: [],
      currentBytes: 0,
      peakBytes: 0,
      createdBuffers: 0,
      createdBytes: 0,
      completedBuffers: 0,
      completedBytes: 0,
      events: [],
    },
  };
  const encoderStates = new WeakMap();
  const passStates = new WeakMap();
  const bufferSizes = new WeakMap();
  const textureSizes = new WeakMap();

  const counters = () => zeroCounters();
  const add = (target, source) => {
    for (const key of Object.keys(source)) target[key] += source[key];
  };
  const delta = (current, previous) => {
    const result = zeroCounters();
    for (const key of Object.keys(result)) result[key] = current[key] - previous[key];
    return result;
  };
  const copy = value => JSON.parse(JSON.stringify(value));

  if (globalThis.navigator?.gpu &&
      typeof globalThis.navigator.gpu.requestAdapter === 'function') {
    const originalRequestAdapter =
      globalThis.navigator.gpu.requestAdapter.bind(globalThis.navigator.gpu);
    globalThis.navigator.gpu.requestAdapter = async (...args) => {
      const adapter = await originalRequestAdapter(...args);
      if (adapter?.info) {
        state.adapterInfo = {
          vendor: adapter.info.vendor || '',
          architecture: adapter.info.architecture || '',
          device: adapter.info.device || '',
          description: adapter.info.description || '',
        };
      }
      return adapter;
    };
  }

  const encoderState = encoder => {
    let value = encoderStates.get(encoder);
    if (!value) {
      value = {
        at: performance.now(),
        counters: counters(),
        pipelines: new Set(),
        recorded: false,
      };
      encoderStates.set(encoder, value);
    }
    return value;
  };
  const recordEncoder = encoder => {
    const currentEncoder = encoderState(encoder);
    if (currentEncoder.recorded) {
      return;
    }
    currentEncoder.recorded = true;
    currentEncoder.counters.commandBuffers = 1;
    currentEncoder.counters.uniquePipelines = currentEncoder.pipelines.size;
    add(state.totals, currentEncoder.counters);
    const snapshot = copy(state.totals);
    const deltaCounters = delta(snapshot, state.previousGlobal);
    state.previousGlobal = snapshot;
    state.records.push({
      at: performance.now(),
      sequence: ++state.sequence,
      counters: deltaCounters,
      stagingInFlightBytes: state.staging.currentBytes,
    });
  };

  const recordStagingCreate = (buffer, descriptor) => {
    if (!buffer || !globalThis.GPUBufferUsage) {
      return;
    }
    const usage = Number(descriptor && descriptor.usage || 0);
    const usageFlags = globalThis.GPUBufferUsage;
    const copiesToAndFromGpu =
      (usage & usageFlags.COPY_SRC) !== 0 && (usage & usageFlags.COPY_DST) !== 0;
    const otherwiseBindable =
      (usage & (usageFlags.VERTEX | usageFlags.INDEX | usageFlags.UNIFORM |
        usageFlags.STORAGE | usageFlags.MAP_READ)) !== 0;
    if (!copiesToAndFromGpu || otherwiseBindable) {
      return;
    }
    const size = Number(descriptor.size || 0);
    const now = performance.now();
    state.staging.pending.push({buffer, size});
    state.staging.currentBytes += size;
    state.staging.peakBytes = Math.max(
      state.staging.peakBytes, state.staging.currentBytes);
    state.staging.createdBuffers++;
    state.staging.createdBytes += size;
    state.staging.events.push({
      at: now,
      type: 'create',
      bytes: size,
      inFlightBytesAfter: state.staging.currentBytes,
    });
  };

  const recordStagingCompletion = status => {
    const pending = state.staging.pending.shift();
    if (!pending) {
      return;
    }
    state.staging.currentBytes = Math.max(
      0, state.staging.currentBytes - pending.size);
    state.staging.completedBuffers++;
    state.staging.completedBytes += pending.size;
    state.staging.events.push({
      at: performance.now(),
      type: 'complete',
      status,
      bytes: pending.size,
      inFlightBytesAfter: state.staging.currentBytes,
    });
  };

  const wrap = (prototype, name, before, after) => {
    if (!prototype || typeof prototype[name] !== 'function') return;
    const original = prototype[name];
    prototype[name] = function(...args) {
      const beforeResult = before ? before(this, args) : undefined;
      const result = original.apply(this, args);
      if (after) after(this, args, result, beforeResult);
      return result;
    };
  };

  const bytesPerBlock = format => {
    const normalized = String(format || '').toLowerCase();
    if (normalized.includes('rgba32')) return [16, 1, 1];
    if (normalized.includes('rgba16float') || normalized.includes('rgba16uint') ||
        normalized.includes('rgba16sint') || normalized.includes('rgba16unorm') ||
        normalized.includes('rgba16snorm')) return [8, 1, 1];
    if (normalized.includes('rgba8') || normalized.includes('bgra8') ||
        normalized.includes('rgb10a2')) return [4, 1, 1];
    if (normalized.includes('rg32')) return [8, 1, 1];
    if (normalized.includes('rg16float') || normalized.includes('rg16uint') ||
        normalized.includes('rg16sint') || normalized.includes('rg16unorm') ||
        normalized.includes('rg16snorm')) return [4, 1, 1];
    if (normalized.includes('depth24plus-stencil8')) return [4, 1, 1];
    if (normalized.includes('rg8') || normalized.includes('depth24plus') ||
        normalized.includes('depth32float')) return [2, 1, 1];
    if (normalized.startsWith('r32') || normalized.includes('rg11b10ufloat')) return [4, 1, 1];
    if (normalized.includes('rgb9e5ufloat')) return [4, 1, 1];
    if (normalized.startsWith('r16') || normalized.startsWith('depth16')) return [2, 1, 1];
    if (normalized.startsWith('stencil8')) return [1, 1, 1];
    if (normalized.startsWith('r8')) return [1, 1, 1];
    if (normalized.startsWith('bc1') || normalized.startsWith('bc4') ||
        normalized.startsWith('etc2-rgb8')) return [8, 4, 4];
    if (normalized.startsWith('bc2') || normalized.startsWith('bc3') ||
        normalized.startsWith('bc5') || normalized.startsWith('bc6') ||
        normalized.startsWith('bc7') || normalized.startsWith('etc2-rgba8')) {
      return [16, 4, 4];
    }
    const astc = /^astc-(\d+)x(\d+)/.exec(normalized);
    if (astc) return [16, Number(astc[1]), Number(astc[2])];
    return null;
  };

  const textureBytes = descriptor => {
    const info = bytesPerBlock(descriptor.format);
    if (!info) {
      state.unknownTextureSizeCount++;
      return 0;
    }
    const [bytes, blockWidth, blockHeight] = info;
    const depth = descriptor.depthOrArrayLayers || 1;
    const samples = descriptor.sampleCount || 1;
    const mipLevelCount = descriptor.mipLevelCount || 1;
    let total = 0;
    for (let mip = 0; mip < mipLevelCount; ++mip) {
      const width = Math.max(1, Math.ceil((descriptor.width || 0) / (2 ** mip)));
      const height = Math.max(1, Math.ceil((descriptor.height || 0) / (2 ** mip)));
      total += Math.ceil(width / blockWidth) * Math.ceil(height / blockHeight) *
        bytes * depth * samples;
    }
    return total;
  };

  const textureCopyBytes = (texture, extent) => {
    const info = bytesPerBlock(texture && texture.format);
    if (!info) return 0;
    const [bytes, blockWidth, blockHeight] = info;
    return Math.ceil((extent.width || 0) / blockWidth) *
      Math.ceil((extent.height || 1) / blockHeight) *
      (extent.depthOrArrayLayers || 1) * bytes;
  };

  wrap(globalThis.GPUCommandEncoder.prototype, 'beginRenderPass',
    (encoder, args) => {
      encoderState(encoder).counters.renderPasses++;
    },
    (encoder, args, pass) => {
      if (pass) passStates.set(pass, encoderState(encoder));
    });
  wrap(globalThis.GPUCommandEncoder.prototype, 'beginComputePass',
    encoder => {
      encoderState(encoder).counters.computePasses++;
    },
    (encoder, args, pass) => {
      if (pass) passStates.set(pass, encoderState(encoder));
    });
  wrap(globalThis.GPUCommandEncoder.prototype, 'finish', null, encoder => recordEncoder(encoder));

  if (globalThis.GPURenderPassEncoder) {
    wrap(globalThis.GPURenderPassEncoder.prototype, 'draw', (pass, args) => {
      const target = passStates.get(pass);
      if (!target) return;
      const vertices = Number(args[0] || 0);
      const instances = Number(args[1] ?? 1);
      target.counters.drawCalls++;
      target.counters.instancesDrawn += instances;
      target.counters.verticesDrawn += vertices * instances;
    });
    wrap(globalThis.GPURenderPassEncoder.prototype, 'drawIndexed', (pass, args) => {
      const target = passStates.get(pass);
      if (!target) return;
      const indices = Number(args[0] || 0);
      const instances = Number(args[1] ?? 1);
      target.counters.drawCalls++;
      target.counters.indexedDrawCalls++;
      target.counters.instancesDrawn += instances;
      target.counters.indicesDrawn += indices * instances;
    });
  }
  for (const prototype of [globalThis.GPURenderPassEncoder, globalThis.GPUComputePassEncoder]) {
    wrap(prototype && prototype.prototype, 'setPipeline', (pass, args) => {
      const target = passStates.get(pass);
      if (!target) return;
      target.counters.pipelineBinds++;
      if (args[0] && typeof args[0] === 'object') target.pipelines.add(args[0]);
    });
    wrap(prototype && prototype.prototype, 'setBindGroup', pass => {
      const target = passStates.get(pass);
      if (target) target.counters.bindGroupBinds++;
    });
  }
  if (globalThis.GPURenderPassEncoder) {
    wrap(globalThis.GPURenderPassEncoder.prototype, 'setVertexBuffer', pass => {
      const target = passStates.get(pass);
      if (target) target.counters.vertexBufferBinds++;
    });
    wrap(globalThis.GPURenderPassEncoder.prototype, 'setIndexBuffer', pass => {
      const target = passStates.get(pass);
      if (target) target.counters.indexBufferBinds++;
    });
  }

  wrap(globalThis.GPUCommandEncoder.prototype, 'copyTextureToTexture',
    (encoder, args) => {
      const target = encoderState(encoder);
      target.counters.copyOperations++;
      target.counters.copiedBytes += textureCopyBytes(args[0] && args[0].texture, args[2] || {});
    });
  wrap(globalThis.GPUCommandEncoder.prototype, 'copyBufferToBuffer',
    (encoder, args) => {
      const target = encoderState(encoder);
      target.counters.copyOperations++;
      target.counters.copiedBytes += Number(args[4] || 0);
    });
  wrap(globalThis.GPUCommandEncoder.prototype, 'copyBufferToTexture',
    (encoder, args) => {
      const target = encoderState(encoder);
      target.counters.copyOperations++;
      target.counters.copiedBytes += textureCopyBytes(args[1] && args[1].texture, args[2] || {});
    });
  wrap(globalThis.GPUCommandEncoder.prototype, 'copyTextureToBuffer',
    (encoder, args) => {
      const target = encoderState(encoder);
      target.counters.copyOperations++;
      target.counters.copiedBytes += textureCopyBytes(args[0] && args[0].texture, args[2] || {});
    });
  wrap(globalThis.GPUCommandEncoder.prototype, 'clearBuffer', (encoder, args) => {
    const target = encoderState(encoder);
    target.counters.copyOperations++;
    const bufferSize = Number(args[0] && args[0].size || 0);
    const offset = Number(args[1] || 0);
    target.counters.copiedBytes += args[2] === undefined
      ? Math.max(0, bufferSize - offset)
      : Number(args[2]);
  });

  wrap(globalThis.GPUQueue && globalThis.GPUQueue.prototype, 'writeBuffer',
    (queue, args) => {
      state.totals.uploadOperations++;
      const data = args[2];
      const size = args[4] ?? (data && data.byteLength ? data.byteLength - Number(args[3] || 0) : 0);
      state.totals.uploadedBytes += Number(size || 0);
    });
  wrap(globalThis.GPUQueue && globalThis.GPUQueue.prototype, 'writeTexture',
    (queue, args) => {
      state.totals.uploadOperations++;
      const data = args[1];
      state.totals.uploadedBytes += Number(data && data.byteLength ? data.byteLength : 0);
      const destination = args[0] || {};
      const size = args[3] || {};
      state.totals.uploadedPayloadBytes += textureCopyBytes(destination.texture, {
        width: size.width,
        height: size.height,
        depthOrArrayLayers: size.depthOrArrayLayers || 1,
      });
    });
  wrap(globalThis.GPUQueue && globalThis.GPUQueue.prototype, 'copyExternalImageToTexture',
    (queue, args) => {
      state.totals.uploadOperations++;
      const destination = args[1] || {};
      const texture = destination.texture;
      const size = args[2] || {width: texture && texture.width, height: texture && texture.height};
      const bytes = textureCopyBytes(texture, {
        width: size.width, height: size.height, depthOrArrayLayers: size.depthOrArrayLayers || 1,
      });
      state.totals.uploadedBytes += bytes;
      state.totals.uploadedPayloadBytes += bytes;
    });
  wrap(globalThis.GPUQueue && globalThis.GPUQueue.prototype, 'submit', queue => {
    state.totals.commandBufferSubmits++;
  });

  if (globalThis.GPUDevice) {
    wrap(globalThis.GPUDevice.prototype, 'createBuffer', (device, args) => {
      state.totals.bufferCreates++;
      const size = Number(args[0] && args[0].size || 0);
      state.totals.logicalResourceBytesCreated += size;
      state.liveResourceBytes += size;
      state.peakLiveResourceBytes = Math.max(state.peakLiveResourceBytes, state.liveResourceBytes);
      return size;
    }, (device, args, buffer, size) => {
      if (buffer && typeof buffer === 'object') {
        bufferSizes.set(buffer, size || 0);
        recordStagingCreate(buffer, args[0] || {});
      }
    });
    wrap(globalThis.GPUDevice.prototype, 'createTexture', (device, args) => {
      state.totals.textureCreates++;
      const size = textureBytes(args[0] || {});
      state.totals.logicalResourceBytesCreated += size;
      state.liveResourceBytes += size;
      state.peakLiveResourceBytes = Math.max(state.peakLiveResourceBytes, state.liveResourceBytes);
      return size;
    }, (device, args, texture, size) => {
      if (texture && typeof texture === 'object') textureSizes.set(texture, size || 0);
    });
    wrap(globalThis.GPUDevice.prototype, 'createRenderPipeline', () => {
      state.totals.renderPipelineCreates++;
    });
    wrap(globalThis.GPUDevice.prototype, 'createRenderPipelineAsync', () => {
      state.totals.renderPipelineCreates++;
    });
    wrap(globalThis.GPUDevice.prototype, 'createComputePipeline', () => {
      state.totals.computePipelineCreates++;
    });
    wrap(globalThis.GPUDevice.prototype, 'createComputePipelineAsync', () => {
      state.totals.computePipelineCreates++;
    });
    wrap(globalThis.GPUDevice.prototype, 'createBindGroup', () => {
      state.totals.bindGroupCreates++;
    });
    wrap(globalThis.GPUDevice.prototype, 'createShaderModule', () => {
      state.totals.shaderModuleCreates++;
    });
  }
  wrap(globalThis.GPUBuffer && globalThis.GPUBuffer.prototype, 'destroy', buffer => {
    const size = bufferSizes.get(buffer) || 0;
    if (size > 0) {
      bufferSizes.delete(buffer);
      state.totals.logicalResourceBytesReleased += size;
      state.liveResourceBytes = Math.max(0, state.liveResourceBytes - size);
    }
  });
  wrap(globalThis.GPUTexture && globalThis.GPUTexture.prototype, 'destroy', texture => {
    const size = textureSizes.get(texture) || 0;
    if (size > 0) {
      textureSizes.delete(texture);
      state.totals.logicalResourceBytesReleased += size;
      state.liveResourceBytes = Math.max(0, state.liveResourceBytes - size);
    }
  });
  wrap(globalThis.GPUCanvasContext && globalThis.GPUCanvasContext.prototype, 'getCurrentTexture', () => {
    state.totals.canvasTextureAcquisitions++;
  });
  if (globalThis.GPUQueue && globalThis.GPUQueue.prototype &&
      typeof globalThis.GPUQueue.prototype.onSubmittedWorkDone === 'function') {
    const originalOnSubmittedWorkDone = globalThis.GPUQueue.prototype.onSubmittedWorkDone;
    globalThis.GPUQueue.prototype.onSubmittedWorkDone = function(...args) {
      const result = originalOnSubmittedWorkDone.apply(this, args);
      if (state.staging.pending.length > 0 && result && typeof result.then === 'function') {
        result.then(
          () => recordStagingCompletion('success'),
          () => recordStagingCompletion('rejected'));
      }
      return result;
    };
  }

  globalThis.__tgfxGPUCounterProbe = {
    snapshot() {
      const records = state.records.map(record => ({...record}));
      return {
        schemaVersion: 'tgfx-benchmark.webgpu-counters.v1',
        createdAt: state.createdAt,
        totals: copy(state.totals),
        records,
        liveResourceBytes: state.liveResourceBytes,
        peakLiveResourceBytes: state.peakLiveResourceBytes,
        unknownTextureSizeCount: state.unknownTextureSizeCount,
        adapterInfo: state.adapterInfo,
        staging: {
          currentBytes: state.staging.currentBytes,
          peakBytes: state.staging.peakBytes,
          pendingBuffers: state.staging.pending.length,
          createdBuffers: state.staging.createdBuffers,
          createdBytes: state.staging.createdBytes,
          completedBuffers: state.staging.completedBuffers,
          completedBytes: state.staging.completedBytes,
          events: state.staging.events.map(event => ({...event})),
        },
        definitions: {
          drawCalls: 'Total draw and drawIndexed calls; indexedDrawCalls is a subset.',
          uniquePipelines: 'Distinct pipelines bound within each command buffer.',
          uploadedBytes: 'CPU source bytes passed to queue upload calls.',
          uploadedPayloadBytes: 'Logical texel bytes for texture uploads, excluding row padding.',
          liveResourceBytes:
            'Upper-bound logical bytes for explicitly destroyed buffer/texture objects; GC is not observed.',
          stagingInFlightBytes:
            'Bytes for GPUBuffers created with COPY_DST|COPY_SRC and released when the matching onSubmittedWorkDone Promise settles.',
        },
      };
    },
  };
}

function quantile(values, fraction) {
  if (values.length === 0) return null;
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.min(sorted.length - 1, Math.max(0, Math.ceil(sorted.length * fraction) - 1))];
}

function intervalSummary(summary) {
  const samples = Array.isArray(summary.samples) ? summary.samples : [];
  const intervals = [];
  for (let index = 1; index < samples.length; ++index) {
    intervals.push(
      (samples[index].callbackBeginUs - samples[index - 1].callbackBeginUs) / 1000);
  }
  return {
    units: {
      interval: 'ms',
      callbackCpu: 'us',
    },
    sampleCount: samples.length,
    intervalCount: intervals.length,
    p50Ms: quantile(intervals, 0.5),
    p95Ms: quantile(intervals, 0.95),
    p99Ms: quantile(intervals, 0.99),
    maxMs: intervals.length > 0 ? Math.max(...intervals) : null,
    over20Ms: intervals.filter(value => value > 20).length,
    over35Ms: intervals.filter(value => value > 35).length,
    callbackCpuUsP50: quantile(
      samples.map(sample => sample.callbackEndUs - sample.callbackBeginUs), 0.5),
  };
}

function stagingWindow(staging, startAt, endAt) {
  const events = (staging.events || []).filter(event => event.at <= endAt);
  let inFlightBytes = 0;
  let peakBytes = 0;
  let createdBuffers = 0;
  let createdBytes = 0;
  let completedBuffers = 0;
  let completedBytes = 0;
  for (const event of events) {
    if (event.at < startAt) {
      inFlightBytes = event.inFlightBytesAfter;
      continue;
    }
    inFlightBytes = event.inFlightBytesAfter;
    peakBytes = Math.max(peakBytes, inFlightBytes);
    if (event.type === 'create') {
      createdBuffers++;
      createdBytes += event.bytes;
    } else if (event.type === 'complete') {
      completedBuffers++;
      completedBytes += event.bytes;
    }
  }
  return {
    units: 'bytes; copies of COPY_DST|COPY_SRC GPUBuffers until onSubmittedWorkDone settles',
    createdBuffers,
    createdBytes,
    completedBuffers,
    completedBytes,
    peakBytes,
    endBytes: inFlightBytes,
    events: events.filter(event => event.at >= startAt),
  };
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

async function collectServedIdentity(baseUrl, packageId, identity) {
  const artifacts = identity.artifacts || {};
  return {
    entry: await hashServedArtifact(baseUrl, packageId, artifacts.entry),
    glue: await hashServedArtifact(baseUrl, packageId, artifacts.glue),
    wasm: await hashServedArtifact(baseUrl, packageId, artifacts.wasm),
  };
}

function aggregate(records) {
  const fields = records.length > 0 ? Object.keys(records[0].counters) : [];
  const result = {};
  for (const field of fields) {
    const values = records.map(record => Number(record.counters[field] || 0));
    result[field] = {
      total: values.reduce((sum, value) => sum + value, 0),
      p50: quantile(values, 0.5),
      p95: quantile(values, 0.95),
      max: values.length > 0 ? Math.max(...values) : null,
    };
  }
  return result;
}

async function run(args) {
  args = normalizeArgs(args);
  const {chromium} = loadPlaywright();
  const manifestPath = path.resolve(repoRoot, `../packages/${args.packageId}/tgfx-build.json`);
  const manifestHash = sha256(fs.readFileSync(manifestPath));
  const identity = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  if (identity.packageId !== args.packageId || identity.backend !== 'webgpu' ||
      !['aot', 'jit'].includes(identity.codegen)) {
    throw new Error(`Invalid WebGPU package identity: ${manifestPath}`);
  }
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.CHROMIUM_PATH || chromium.executablePath(),
    args: [
      '--no-sandbox',
      '--enable-unsafe-webgpu',
      '--disable-gpu-shader-disk-cache',
      '--disk-cache-size=1',
    ],
  });
  try {
    const page = await browser.newPage({viewport: {width: 1000, height: 700}, deviceScaleFactor: 1});
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
    await page.addInitScript(installWebGPUCounterProbe);
    const params = new URLSearchParams({
      benchmark: '1',
      case: args.caseId,
      count: String(args.count),
      width: '900',
      height: '600',
      recorder: '1',
      backend: 'webgpu',
      aot: identity.codegen === 'aot' ? '1' : '0',
      testIntent: args.testIntent,
      runNonce: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
    });
    if (args.testIntent === 'cold-start') {
      params.set('initialWindowFrames', String(args.initialWindowFrames));
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
    const url = `${args.baseUrl}/packages/${args.packageId}/index.html?${params}`;
    await page.goto(url, {waitUntil: 'load', timeout: 30000});
    await page.waitForFunction(
      () => window.__tgfxBenchmark?.benchmarkSummary !== null,
      null,
      {timeout: Math.max(30000, args.measurementMs + 30000)},
    );
    const measured = await page.evaluate(() => ({
      summary: window.__tgfxBenchmark.benchmarkSummary,
      readyAt: window.__tgfxBenchmark.readyAt,
      benchmarkCompleteAt: window.__tgfxBenchmark.benchmarkCompleteAt,
      probe: window.__tgfxGPUCounterProbe?.snapshot() ?? null,
      runtimeAOT: window.__tgfxBenchmarkView?.getAOTEvidence?.() ?? null,
      benchmarkConfig: window.__tgfxBenchmarkConfig || null,
      userAgent: navigator.userAgent,
      devicePixelRatio: window.devicePixelRatio,
      pageViewport: {width: window.innerWidth, height: window.innerHeight},
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
    if (!measured.probe) {
      throw new Error('WebGPU counter probe did not install');
    }
    const steadyStartDelayMs = args.testIntent === 'cold-start'
      ? 0 : Number(measured.summary.warmupDurationUs || 0) / 1000;
    const measuredAfter = measured.readyAt === null
      ? measured.probe.createdAt : measured.readyAt + steadyStartDelayMs;
    const measuredBefore = measured.benchmarkCompleteAt ?? Number.POSITIVE_INFINITY;
    const records = measured.probe.records.filter(
      record => record.at >= measuredAfter && record.at <= measuredBefore);
    if (records.length === 0) {
      throw new Error('No WebGPU command-buffer counter records were collected in the measurement window');
    }
    const counters = aggregate(records);
    if (Number(counters.renderPasses?.total || 0) === 0 ||
        Number(counters.drawCalls?.total || 0) === 0 ||
        Number(counters.commandBufferSubmits?.total || 0) === 0) {
      throw new Error('WebGPU counter probe collected no observable render/draw/submit work');
    }
    const servedIdentity = await collectServedIdentity(
        args.baseUrl, args.packageId, identity);
    const samples = Array.isArray(measured.summary.samples)
      ? measured.summary.samples : [];
    const actualCounts = [...new Set(samples.map(sample => sample.sceneCount))];
    const consoleErrorCount = consoleMessages.filter(
        message => message.type === 'error').length;
    const expectedAOT = identity.codegen === 'aot';
    const aotValidity = expectedAOT
      ? measured.runtimeAOT?.isLoaded === true
      : measured.runtimeAOT?.isLoaded !== true;
    const runtimeModeValidity = expectedAOT
      ? measured.runtimeAOT?.isLoaded === true &&
        measured.benchmarkConfig?.aotRequested === true
      : measured.benchmarkConfig?.aotRequested === false &&
        measured.runtimeAOT?.isLoaded !== true;
    const actualBackendValid =
      Number(counters.renderPasses?.total || 0) > 0 &&
      Number(counters.drawCalls?.total || 0) > 0 &&
      Number(counters.commandBufferSubmits?.total || 0) > 0;
    const output = {
      schemaVersion: 'tgfx-benchmark.webgpu-counter-run.v1',
      packageId: args.packageId,
      manifestSha256: manifestHash,
      servedIdentity,
      packageIdentity: {
        packageRole: identity.packageRole || 'official',
        basePackageId: identity.basePackageId ?? null,
        baseCommit: identity.baseCommit ?? null,
        atRemoteRef: identity.atRemoteRef ?? null,
        sourceDiff: identity.sourceDiff ?? null,
        codegen: identity.codegen,
        backend: identity.backend,
        commit: identity.commit,
        dirty: identity.dirty,
        builtAt: identity.builtAt,
        artifacts: identity.artifacts,
        buildPatches: identity.buildPatches,
        benchmark: identity.benchmark,
        sourceName: identity.sourceName,
      },
      caseId: args.caseId,
      count: args.count,
      testIntent: args.testIntent,
      initialWindowFrames: args.initialWindowFrames,
      measurementMs: args.measurementMs,
      webgpuCoalesceUploads: args.webgpuCoalesceUploads,
      webgpuDynamicBindGroups: args.webgpuDynamicBindGroups,
      compactRRectInstances: args.compactRRectInstances,
      url,
      executionStatus: measured.summary.executionStatus,
      recordedFrameCount: measured.summary.recordedFrameCount,
      sampleCount: measured.summary.sampleCount,
      actualCounts,
      actualBackendValid,
      aotValidity,
      runtimeModeValidity,
      contractValid:
        measured.summary.executionStatus === 'completed' &&
        samples.length > 0 &&
        actualCounts.length === 1 &&
        actualCounts[0] === args.count &&
        actualBackendValid &&
        aotValidity &&
        runtimeModeValidity &&
        pageErrors.length === 0 &&
        consoleErrorCount === 0,
      frameInterval: intervalSummary(measured.summary),
      drainAfterMeasurementRaw: {
        value: measured.summary.drainAfterMeasurementUs,
        note: 'Preserved raw field; this is not GPU completion time.',
      },
      measurementWindow: {
        startAt: measuredAfter,
        endAt: measured.benchmarkCompleteAt,
        basis: args.testIntent === 'cold-start'
          ? 'readyAt to cold-start-complete callback'
          : 'readyAt + actual steady warmup to benchmark-complete callback',
      },
      counterRecordCount: records.length,
      counters,
      counterDefinitions: measured.probe.definitions,
      resourceMemory: {
        scope: 'page-lifetime until snapshot, not the measurement-window delta',
        liveLogicalBytes: measured.probe.liveResourceBytes,
        peakLogicalBytes: measured.probe.peakLiveResourceBytes,
        createdLogicalBytes: measured.probe.totals.logicalResourceBytesCreated,
        releasedLogicalBytes: measured.probe.totals.logicalResourceBytesReleased,
        unknownTextureSizeCount: measured.probe.unknownTextureSizeCount,
      },
      stagingInFlight: {
        pageLifetime: measured.probe.staging,
        measurementWindow: stagingWindow(
          measured.probe.staging, measuredAfter, measuredBefore),
      },
      runtimeAOT: measured.runtimeAOT,
      environment: {
        browserVersion: browser.version(),
        chromiumExecutable: process.env.CHROMIUM_PATH || chromium.executablePath(),
        userAgent: measured.userAgent,
        devicePixelRatio: measured.devicePixelRatio,
        pageViewport: measured.pageViewport,
        canvas: measured.canvas,
        adapterInfo: measured.probe.adapterInfo,
        benchmarkConfig: measured.benchmarkConfig,
      },
      pageErrors,
      consoleMessages,
      browserErrors: pageErrors.map(error => error.message),
    };
    const json = `${JSON.stringify(output, null, 2)}\n`;
    if (args.output) {
      fs.mkdirSync(path.dirname(path.resolve(args.output)), {recursive: true});
      fs.writeFileSync(path.resolve(args.output), json);
    }
    if (expectedAOT && !aotValidity) {
      throw new Error('AOT runtime identity is not loaded');
    }
    if (actualCounts.length !== 1 || actualCounts[0] !== args.count) {
      throw new Error(`Actual count mismatch: ${actualCounts.join(',')}`);
    }
    if (consoleErrorCount > 0) {
      throw new Error(`Browser console errors: ${consoleErrorCount}`);
    }
    if (pageErrors.length > 0) {
      throw new Error(
          `Browser errors: ${pageErrors.map(error => error.message).join('; ')}`);
    }
    return output;
  } finally {
    await browser.close();
  }
}

async function main() {
  const output = await run(parseArgs(process.argv));
  process.stdout.write(`${JSON.stringify(output, null, 2)}\n`);
  return output;
}

module.exports = {
  aggregate,
  defaultArgs,
  installWebGPUCounterProbe,
  normalizeArgs,
  parseArgs,
  run,
};

if (require.main === module) {
  main().catch(error => {
    console.error(error);
    process.exitCode = 1;
  });
}
