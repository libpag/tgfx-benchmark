'use strict';

const $ = id => document.getElementById(id);
const ui = {
  backend: $('backend'), scene: $('scene'), count: $('count'),
  engineMode: $('engine-mode'), engineIdentity: $('engine-identity'),
  packageSource: $('package-source'), refreshPackages: $('refresh-packages'),
  antialias: $('antialias'), stroke: $('stroke'),
  measurement: $('measurement'),
  targetFps: $('target-fps'),
  run: $('run'), stop: $('stop'), cancel: $('cancel'), reset: $('reset'),
  frame: $('scene-frame'), stage: $('scene-stage'),
};
const state = {
  view: 'scene', testIntent: 'steady-state', runMode: 'ramp', running: false, timer: null,
  startedAt: 0, generation: 0, config: null, report: null, history: [],
  engineMode: 'aot', packageIdentity: null, identityCache: new Map(), starting: false,
  packageCatalog: [], packageSelections: {},
  adaptive: null,
  antialias: true, stroke: false,
  lastProgressFrame: null, lastProgressAt: 0,
};
const defaults = {
  'coldstart.shader-mix': 256,
  'legacy.particles-rect': 600,
  'legacy.particles-circle': 600,
  'legacy.particles-oval': 600,
  'legacy.particles-rrect': 600,
  'legacy.particles-star': 300,
  'motionmark.multiply': 512,
  'motionmark.canvas-arcs': 8,
  'motionmark.leaves': 128,
  'motionmark.canvas-lines': 16,
  'motionmark.images': 64,
  'motionmark.design': 8,
  'motionmark.suits': 24,
};
const limits = {
  'coldstart.shader-mix': 10000,
  'legacy.particles-rect': 1000000,
  'legacy.particles-circle': 1000000,
  'legacy.particles-oval': 1000000,
  'legacy.particles-rrect': 1000000,
  'legacy.particles-star': 1000000,
  'motionmark.multiply': 10000, 'motionmark.canvas-arcs': 10000,
  'motionmark.leaves': 10000,
  'motionmark.canvas-lines': 10000, 'motionmark.images': 10000,
  'motionmark.design': 2048, 'motionmark.suits': 10000,
};
const coldStartScenes = new Set(['coldstart.shader-mix']);
const legacySceneHints = {
  'legacy.particles-rect': '原版 TGFX 矩形粒子：围绕指针持续发射矩形，并测试矩形填充或描边路径。',
  'legacy.particles-circle': '原版 TGFX 圆形粒子：围绕指针持续发射圆形，并测试圆形绘制路径。',
  'legacy.particles-oval': '原版 TGFX 椭圆粒子：围绕指针持续发射椭圆，并测试椭圆绘制路径。',
  'legacy.particles-rrect': '原版 TGFX 圆角矩形粒子：围绕指针持续发射圆角矩形，并测试圆角矩形绘制路径。',
  'legacy.particles-star': '原版 TGFX 星形粒子：围绕指针持续发射星形路径，并测试路径填充或描边。',
};
const shaderMixGroups = [
  'solid-aa-rrect',
  'complex-non-aa-rrect',
  'round-stroke-hairline',
  'linear-radial-conic-diamond-gradient',
  'image-sampling',
  'tiled-texture',
  'text-atlas-clip',
  'color-filter',
  'path-and-mask',
  'blend-and-clip',
  'perlin-noise',
  'blur-shadow-mask-filter',
  'mesh',
  'batch-instancing',
];
const shaderMixGroupLabels = {
  'solid-aa-rrect': 'AA 圆角矩形',
  'complex-non-aa-rrect': '复杂 / 非 AA 圆角',
  'round-stroke-hairline': '描边 / 发丝线',
  'linear-radial-conic-diamond-gradient': '线性 / 径向 / 锥形 / 菱形渐变',
  'image-sampling': '图像采样',
  'tiled-texture': '平铺纹理',
  'text-atlas-clip': '文字图集 / 裁剪',
  'color-filter': 'ColorFilter',
  'path-and-mask': 'Path / Mask',
  'blend-and-clip': 'Blend / 离屏裁剪',
  'perlin-noise': 'Perlin / Turbulence 噪声',
  'blur-shadow-mask-filter': 'Blur / Shadow / MaskFilter',
  'mesh': 'Mesh',
  'batch-instancing': 'Batch / Instancing',
};
const isLegacyScene = caseId => caseId.startsWith('legacy.particles-');
const countUnit = caseId => caseId === 'coldstart.shader-mix'
  ? '个绘制单元' : caseId === 'motionmark.design' ? '个文字层'
    : isLegacyScene(caseId) ? '个粒子' : '个对象';
const engineLabel = mode => mode === 'aot' ? 'AOT' : 'JIT';
const backendLabel = backend => backend === 'webgpu' ? 'WebGPU' : 'WebGL';
const packageIdFor = options => options.tgfxPackage || `${options.engineMode}-${options.backend}`;
const currentBasePackageId = () => `${state.engineMode}-${ui.backend.value}`;
const selectedPackageId = () => ui.packageSource.value || currentBasePackageId();
const shortCommit = commit => commit ? commit.slice(0, 8) : 'unknown';

function selectedSceneName() {
  return ui.scene.selectedOptions[0].textContent;
}

function config() {
  return {
    backend: ui.backend.value,
    engineMode: state.engineMode,
    tgfxPackage: selectedPackageId(),
    tgfxIdentity: state.packageIdentity,
    caseId: ui.scene.value,
    sceneName: selectedSceneName(),
    count: Number(ui.count.value),
    testIntent: state.testIntent,
    initialWindowFrames: 30,
    measurementMs: Number(ui.measurement.value),
    targetFps: Number(ui.targetFps.value),
    mode: state.testIntent === 'cold-start' ? 'cold-start' : state.runMode,
    antialias: isLegacyScene(ui.scene.value) ? state.antialias : null,
    stroke: isLegacyScene(ui.scene.value) ? state.stroke : null,
  };
}

function validate() {
  const countMaximum = state.testIntent === 'steady-state' && state.runMode === 'ramp'
    ? Number.MAX_SAFE_INTEGER : Number(ui.count.max);
  const fields = [[ui.count, countMaximum]];
  if (state.testIntent === 'steady-state' && state.runMode === 'fixed') {
    fields.push([ui.measurement, 60000]);
  }
  if (state.testIntent === 'steady-state' && state.runMode === 'ramp') {
    fields.push([ui.targetFps, 240]);
  }
  for (const [input, maximum] of fields) {
    const number = Number(input.value);
    const minimum = Number(input.min || 1);
    if (!Number.isSafeInteger(number) || number < minimum || number > maximum) {
      showError(`${input.labels[0].textContent}必须是 ${minimum} 至 ${maximum} 之间的整数。`);
      input.focus();
      return false;
    }
  }
  showError('');
  return true;
}

function showError(message) {
  $('error').textContent = message;
  $('error').classList.toggle('hidden', !message);
}

function packageOptionLabel(entry) {
  const role = entry.packageRole === 'official'
    ? '正式' : entry.tag ? `候选 · ${entry.tag}` : '候选';
  const dirty = entry.dirty ? ' · dirty' : '';
  return `${role} · ${shortCommit(entry.commit)}${dirty}`;
}

function syncPackageSelector() {
  const basePackageId = currentBasePackageId();
  const entries = state.packageCatalog.filter(entry =>
    entry.basePackageId === basePackageId);
  const previous = state.packageSelections[basePackageId];
  const available = new Set(entries.map(entry => entry.packageId));
  let selected = available.has(previous) ? previous : null;
  if (!selected) {
    selected = entries.find(entry => entry.packageRole === 'official')?.packageId ||
      entries[0]?.packageId || '';
  }
  const groups = [
    ['正式包', entries.filter(entry => entry.packageRole === 'official')],
    ['候选包', entries.filter(entry => entry.packageRole === 'candidate')],
  ];
  const fragment = document.createDocumentFragment();
  for (const [label, groupEntries] of groups) {
    if (groupEntries.length === 0) continue;
    const group = document.createElement('optgroup');
    group.label = label;
    for (const entry of groupEntries) {
      const option = document.createElement('option');
      option.value = entry.packageId;
      option.textContent = packageOptionLabel(entry);
      option.title = `${entry.packageId}\nTGFX ${entry.commit}\n${entry.builtAt || ''}`;
      group.append(option);
    }
    fragment.append(group);
  }
  if (entries.length === 0) {
    const option = document.createElement('option');
    option.value = '';
    option.textContent = `没有 ${engineLabel(state.engineMode)} ${backendLabel(ui.backend.value)} 运行包`;
    fragment.append(option);
  }
  ui.packageSource.replaceChildren(fragment);
  ui.packageSource.value = selected;
  if (selected) {
    state.packageSelections[basePackageId] = selected;
  } else {
    delete state.packageSelections[basePackageId];
  }
  return entries;
}

async function loadPackageCatalog() {
  let response = await fetch('/api/packages', {cache: 'no-store'});
  if (!response.ok) {
    response = await fetch('./packages/index.json', {cache: 'no-store'});
  }
  if (!response.ok) {
    throw new Error('无法读取运行包目录，请先生成至少一个正式包或候选包。');
  }
  const catalog = await response.json();
  if (catalog.schemaVersion !== 'tgfx-benchmark.package-catalog.v1' ||
      !Array.isArray(catalog.packages)) {
    throw new Error('运行包目录格式无效。');
  }
  state.packageCatalog = catalog.packages;
  state.identityCache.clear();
  syncPackageSelector();
  return catalog;
}

async function loadPackageIdentity(packageId, refresh = false) {
  if (!refresh && state.identityCache.has(packageId)) {
    return state.identityCache.get(packageId);
  }
  const response = await fetch(`./packages/${packageId}/tgfx-build.json`, {cache: 'no-store'});
  if (!response.ok) {
    throw new Error(`缺少 ${packageId} 包，请构建该运行包或选择已有包。`);
  }
  const identity = await response.json();
  if (identity.packageId !== packageId) {
    throw new Error(`${packageId} 的构建身份不匹配。`);
  }
  const catalogEntry = state.packageCatalog.find(entry => entry.packageId === packageId);
  if (catalogEntry &&
      (identity.codegen !== catalogEntry.codegen || identity.backend !== catalogEntry.backend)) {
    throw new Error(`${packageId} 的编译器或后端与包目录不一致。`);
  }
  if (!/^[0-9a-f]{40}$/.test(identity.commit || '')) {
    throw new Error(`${packageId} 缺少有效的 TGFX commit。`);
  }
  state.identityCache.set(packageId, identity);
  return identity;
}

async function updatePackageIdentity() {
  const packageId = selectedPackageId();
  ui.engineIdentity.classList.remove('identity-error');
  ui.engineIdentity.textContent = `正在读取 ${packageId}…`;
  try {
    const identity = await loadPackageIdentity(packageId, true);
    if (packageId !== selectedPackageId()) {
      return state.packageIdentity;
    }
    state.packageIdentity = identity;
    const dirty = identity.dirty ? ' +dirty' : '';
    const role = identity.packageRole === 'candidate' ? '候选' : '正式';
    const tag = identity.tag ? ` · ${identity.tag}` : '';
    ui.engineIdentity.textContent =
      `${role}${tag} · TGFX ${shortCommit(identity.commit)}${dirty} · ` +
      `${engineLabel(identity.codegen)} · ${backendLabel(identity.backend)}`;
    ui.engineIdentity.title = identity.commit || '';
    return identity;
  } catch (error) {
    if (packageId !== selectedPackageId()) {
      return state.packageIdentity;
    }
    state.packageIdentity = null;
    ui.engineIdentity.textContent = error.message;
    ui.engineIdentity.title = '';
    ui.engineIdentity.classList.add('identity-error');
    return null;
  }
}

function pageUrl(options, run = false) {
  const packageId = packageIdFor(options);
  const page = `./packages/${packageId}/index.html`;
  const query = new URLSearchParams({
    case: options.caseId,
    count: String(!run && coldStartScenes.has(options.caseId)
      ? Math.min(options.count, 32) : options.count),
    fluid: '1', backend: options.backend,
    aot: options.engineMode === 'aot' ? '1' : '0',
  });
  if (isLegacyScene(options.caseId)) {
    query.set('aa', options.antialias ? '1' : '0');
    query.set('stroke', options.stroke ? '1' : '0');
  }
  if (run) {
    query.set('benchmark', '1');
    query.set('recorder', '1');
    query.set('testIntent', options.testIntent);
    query.set('runNonce', options.runNonce);
    if (options.testIntent === 'cold-start') {
      query.set('initialWindowFrames', String(options.initialWindowFrames));
    } else if (options.mode === 'ramp') {
      query.set('adaptive', '1');
      query.set('targetFps', String(options.targetFps));
    } else if (options.mode === 'manual') {
      query.set('manual', '1');
    } else {
      query.set('measurementMs', String(options.measurementMs));
    }
  }
  return `${page}?${query}`;
}

function setView(view) {
  if (state.running && view !== 'scene') return;
  if (view === 'scene' && state.view === 'report' &&
      frameWindow()?.__tgfxBenchmark?.benchmarkSummary) {
    ui.frame.src = pageUrl(config());
    $('scene-phase').textContent = '预览';
    $('scene-message').textContent = '正在加载场景…';
    $('hud-first-submit').textContent = '-';
    $('hud-first-visible').textContent = '-';
    $('hud-phase').textContent = '预览';
  }
  state.view = view;
  $('status-state').textContent = state.running
    ? $('live-phase').textContent : view === 'report' && state.report ? '已完成' : '预览';
  $('scene-view').classList.toggle('hidden', view !== 'scene');
  $('report-view').classList.toggle('hidden', view !== 'report');
  document.querySelectorAll('[data-view]').forEach(button => {
    button.setAttribute('aria-selected', String(button.dataset.view === view));
  });
  if (view === 'report') requestAnimationFrame(drawChart);
}

function controls() {
  const locked = state.running || state.starting;
  const coldStart = state.testIntent === 'cold-start';
  const legacy = isLegacyScene(ui.scene.value);
  for (const input of [ui.backend, ui.scene, ui.count, ui.measurement,
    ui.targetFps, ui.reset, ui.packageSource, ui.refreshPackages,
    ui.antialias, ui.stroke]) {
    input.disabled = locked;
  }
  ui.antialias.disabled = locked || !legacy;
  ui.stroke.disabled = locked || !legacy;
  for (const option of ui.scene.options) {
    option.disabled = coldStart && !coldStartScenes.has(option.value);
  }
  document.querySelectorAll('[data-scene]').forEach(button => {
    button.disabled = locked || (coldStart && !coldStartScenes.has(button.dataset.scene));
    button.setAttribute('aria-pressed', String(button.dataset.scene === ui.scene.value));
  });
  document.querySelectorAll('.gallery-steady').forEach(group =>
    group.classList.toggle('hidden', coldStart));
  $('particle-options').classList.toggle('hidden', !legacy);
  document.querySelectorAll('[data-test-intent]').forEach(button => {
    button.disabled = locked;
    button.setAttribute('aria-pressed',
      String(button.dataset.testIntent === state.testIntent));
  });
  ui.engineMode.querySelectorAll('[data-engine-mode]').forEach(button => {
    button.disabled = locked;
    button.setAttribute('aria-pressed',
      String(button.dataset.engineMode === state.engineMode));
  });
  document.querySelectorAll('[data-run-mode]').forEach(button => {
    button.disabled = locked || coldStart;
    button.setAttribute('aria-pressed', String(button.dataset.runMode === state.runMode));
  });
  $('steady-run-mode').classList.toggle('hidden', coldStart);
  $('cold-start-fields').classList.toggle('hidden', !coldStart);
  document.querySelectorAll('.steady-live-only').forEach(row =>
    row.classList.toggle('hidden', coldStart));
  document.querySelectorAll('.cold-live-only').forEach(row =>
    row.classList.toggle('hidden', !coldStart));
  ui.run.classList.toggle('hidden', locked);
  ui.run.textContent = coldStart ? '开始首帧测试' : '开始测量';
  document.querySelectorAll('[data-view="report"]').forEach(button => {
    button.disabled = locked || !state.report;
  });
  ui.stop.classList.toggle('hidden',
    !state.running || coldStart || state.runMode === 'fixed');
  ui.stop.disabled = !state.running;
  ui.stop.textContent = !coldStart && state.runMode === 'ramp' && !state.adaptive?.settled
    ? '结束爬坡并查看当前结果' : '结束并查看报告';
  ui.cancel.classList.toggle('hidden', !state.running);
  $('measurement-field').classList.toggle('hidden',
    coldStart || state.runMode !== 'fixed');
  const unit = ui.scene.value === 'motionmark.design'
    ? '文字层数' : ui.scene.value === 'coldstart.shader-mix' ? '绘制单元数' : '对象数';
  $('objects-label').textContent =
    `${coldStart || state.runMode === 'ramp' ? '初始' : ''}${unit}`;
  $('hud-count-label').textContent = coldStart ? '绘制单元' : unit;
  $('measurement-label').textContent =
    state.runMode === 'ramp' ? '稳定判定时长 (ms)' : '测量时长 (ms)';
  ui.measurement.min = '1';
  $('ramp-fields').classList.toggle('hidden', coldStart || state.runMode !== 'ramp');
  $('ramp-duration').classList.toggle('hidden', coldStart || state.runMode !== 'ramp');
  if (!coldStart && state.runMode === 'ramp') {
    $('ramp-duration').textContent =
      '对象数自动增长并搜索 FPS/p95 边界；连续两个同数量窗口失败后冻结数量，动画继续直到手动结束。';
  }
  $('live-panel').classList.toggle('hidden', !state.running);
  $('status-state').textContent = state.running
    ? $('live-phase').textContent : state.view === 'report' && state.report ? '已完成' : '预览';
}

function updateSceneHeader() {
  if (state.testIntent === 'steady-state' && state.runMode === 'ramp') {
    ui.count.removeAttribute('max');
  } else {
    ui.count.max = String(limits[ui.scene.value] || 2048);
  }
  $('scene-title').textContent = selectedSceneName();
  $('scene-meta').textContent =
    `${engineLabel(state.engineMode)} · ${backendLabel(ui.backend.value)} · ${ui.count.value} ${countUnit(ui.scene.value)}`;
  let hint =
    `${state.testIntent === 'cold-start' ? '首帧测试 · ' : ''}${selectedSceneName()} · ` +
    `${state.testIntent === 'cold-start' || state.runMode === 'ramp' ? '初始 ' : ''}` +
    `${ui.count.value} ${countUnit(ui.scene.value)}`;
  if (ui.scene.value === 'coldstart.shader-mix') {
    hint = `${state.testIntent === 'cold-start' ? '首帧测试 · ' : ''}` +
      `${ui.count.value} 个绘制单元。每个单元是一组可见图形，并选用一种或多种滤镜、遮罩或混合效果，` +
      '用来触发不同的 GPU 绘制程序（shader）；它不是 shader 文件数，也不等于 AOT 命中数。';
  } else if (legacySceneHints[ui.scene.value]) {
    hint = `${legacySceneHints[ui.scene.value]} ` +
      `${state.runMode === 'ramp' ? '初始 ' : ''}${ui.count.value} 个粒子。`;
  }
  $('scene-hint').textContent = hint;
}

function updateSceneCount(count) {
  const backend = state.running ? state.config.backend : ui.backend.value;
  const engineMode = state.running ? state.config.engineMode : state.engineMode;
  const caseId = state.running ? state.config.caseId : ui.scene.value;
  $('scene-meta').textContent =
    `${engineLabel(engineMode)} · ${backendLabel(backend)} · ${count} ${countUnit(caseId)}`;
  $('hud-count').textContent = String(count);
}

async function reloadPreview() {
  if (state.testIntent === 'steady-state' && state.runMode === 'ramp') {
    ui.count.removeAttribute('max');
  } else {
    ui.count.max = String(limits[ui.scene.value] || 2048);
  }
  if (state.running || state.starting || !validate()) return;
  const generation = ++state.generation;
  await updatePackageIdentity();
  if (state.running || state.starting || generation !== state.generation) return;
  updateSceneHeader();
  $('scene-phase').textContent = '预览';
  $('scene-message').textContent = '正在加载场景…';
  ui.frame.src = pageUrl(config());
  $('hud-fps').textContent = '-';
  $('hud-time').textContent = '-';
  $('hud-first-submit').textContent = '-';
  $('hud-first-visible').textContent = '-';
  $('hud-count').textContent = ui.count.value;
  $('hud-phase').textContent = '预览';
  setView('scene');
}

function frameWindow() {
  return ui.frame.contentWindow;
}

function sampleStats() {
  const view = frameWindow()?.__tgfxBenchmarkView;
  if (!view || typeof view.getBenchmarkLiveStatsJson !== 'function') return null;
  const stats = JSON.parse(view.getBenchmarkLiveStatsJson());
  stats.displayRefreshFps = frameWindow()?.__tgfxBenchmarkDisplayFps?.() ?? null;
  return stats;
}

function updateHud(stats) {
  if (!stats) return;
  $('hud-fps').textContent = stats.currentFps > 0 ? stats.currentFps.toFixed(1) : '-';
  $('hud-time').textContent = formatMs(stats.frameCallbackMs);
  $('hud-count').textContent = String(stats.sceneCount || ui.count.value);
  $('hud-phase').textContent = $('scene-phase').textContent;
  $('hud-first-submit').textContent = state.running && stats.firstSubmitUs > 0
    ? formatMs(stats.firstSubmitUs / 1000) : '-';
  $('hud-first-visible').textContent = state.running && stats.coldStartFirstVisibleUs > 0
    ? formatMs(stats.coldStartFirstVisibleUs / 1000) : '-';
}

function updateResolution() {
  const canvas = frameWindow()?.document.querySelector('#benchmark');
  const refresh = frameWindow()?.__tgfxBenchmarkDisplayFps?.();
  $('render-resolution').textContent = canvas
    ? `渲染分辨率：${canvas.width} × ${canvas.height} 像素 (DPR ${frameWindow().devicePixelRatio})` +
      `${refresh ? ` · 浏览器刷新上限 ${refresh.toFixed(0)} 次/秒` : ''}`
    : '渲染分辨率：-';
}

function formatMs(value) {
  return Number.isFinite(value) ? `${value.toFixed(1)} ms` : '-';
}

function formatDuration(value) {
  if (!Number.isFinite(value)) return '-';
  return value < 10000 ? `${Math.round(value)} ms` : `${(value / 1000).toFixed(1)} s`;
}

function quantile(values, fraction) {
  if (!values.length) return null;
  const ordered = [...values].sort((a, b) => a - b);
  return ordered[Math.ceil(ordered.length * fraction) - 1];
}

function phase(samples, field) {
  const values = samples.map(item => item[field] / 1000).filter(Number.isFinite);
  return {p50: quantile(values, .5), p95: quantile(values, .95)};
}

function buildCountTimeline(samples) {
  if (samples.length < 2) return [];
  const startUs = samples[0].callbackBeginUs;
  const bucketUs = 100000;
  const points = [];
  let nextBucket = startUs;
  for (const sample of samples) {
    if (sample.callbackBeginUs < nextBucket) continue;
    points.push({
      elapsedMs: (sample.callbackBeginUs - startUs) / 1000,
      count: sample.sceneCount ?? 0,
    });
    while (nextBucket <= sample.callbackBeginUs) nextBucket += bucketUs;
  }
  const last = samples[samples.length - 1];
  const lastPoint = {
    elapsedMs: (last.callbackBeginUs - startUs) / 1000,
    count: last.sceneCount ?? 0,
  };
  if (!points.length || points[points.length - 1].elapsedMs !== lastPoint.elapsedMs) {
    points.push(lastPoint);
  }
  return points;
}

function lastSameCountWindow(samples, count, maximumSamples = 61) {
  if (!samples.length || !count) return [];
  const latest = [];
  for (let index = samples.length - 1; index >= 0; --index) {
    const sample = samples[index];
    if (sample.sceneCount !== count) break;
    latest.push(sample);
    if (latest.length >= maximumSamples) break;
  }
  return latest.reverse();
}

function summarize(summary, options) {
  const samples = Array.isArray(summary.samples) ? summary.samples : [];
  const testIntent = summary.testIntent === 'cold-start' ? 'cold-start' : 'steady-state';
  const coldStart = testIntent === 'cold-start';
  const finalCount = Number(summary.finalSceneCount ??
    summary.adaptivePeakCount ?? options.count);
  const thresholdCount = summary.adaptiveThresholdReached || summary.adaptiveTargetReached
    ? Number(summary.adaptiveSettledCount) : 0;
  const stableStartUs = summary.adaptive && summary.adaptiveTargetReached
    ? summary.adaptiveStableStartUs : 0;
  const stableSamples = stableStartUs > 0
    ? samples.filter(sample => sample.callbackBeginUs >= stableStartUs &&
        sample.sceneCount === thresholdCount)
    : [];
  let latestSamples = summary.adaptive
    ? lastSameCountWindow(samples, thresholdCount || finalCount)
    : samples;
  if (summary.adaptive && latestSamples.length === 0 && samples.length > 0) {
    latestSamples = lastSameCountWindow(samples, samples[samples.length - 1].sceneCount);
  }
  const analysisCount = latestSamples.length > 0
    ? Number(latestSamples[0].sceneCount) : thresholdCount || finalCount;
  const analyzedSamples = coldStart ? samples : summary.adaptive
    ? summary.adaptiveTargetReached ? stableSamples : latestSamples
    : samples;
  const analysisKind = coldStart ? 'cold-start' : summary.adaptiveTargetReached
    ? 'stable'
    : summary.adaptiveThresholdReached ? 'threshold-estimate' : 'current-load';
  const firstFrame = (() => {
    if (summary.firstFrame?.available === true) {
      return {
        available: true,
        reason: null,
        callbackUs: summary.firstFrame.callbackUs,
        sceneDrawUs: summary.firstFrame.sceneDrawUs,
        flushUs: summary.firstFrame.flushUs,
        submitUs: summary.firstFrame.submitUs,
      };
    }
    if (summary.firstFrame?.available === false) {
      return summary.firstFrame;
    }
    if (!coldStart || samples.length === 0) {
      return {
        available: false,
        reason: coldStart ? 'no-sample' : 'not-cold-start',
        callbackUs: null,
        sceneDrawUs: null,
        flushUs: null,
        submitUs: null,
      };
    }
    return {
      available: true,
      reason: null,
      callbackUs: samples[0].callbackEndUs - samples[0].callbackBeginUs,
      sceneDrawUs: samples[0].sceneDrawUs,
      flushUs: samples[0].flushUs,
      submitUs: samples[0].submitUs,
    };
  })();
  const intervals = analyzedSamples.slice(1).map((sample, i) =>
    (sample.callbackBeginUs - analyzedSamples[i].callbackBeginUs) / 1000);
  const targetBudgetMs = coldStart ? null :
    options.mode === 'ramp' ? 1000 / options.targetFps : 1000 / 60;
  let runtimeEvidence = null;
  try {
    runtimeEvidence = frameWindow()?.__tgfxBenchmarkView?.getAOTEvidence?.() ?? null;
  } catch (error) {
    runtimeEvidence = {available: false, reason: error.message};
  }
  const identity = options.tgfxIdentity;
  const runtimeFailureCount = aotFailureCount(runtimeEvidence);
  const runtimeValidity = identity?.codegen === 'aot'
    ? runtimeEvidence?.available === true && runtimeEvidence?.isLoaded === true &&
      runtimeFailureCount === 0
    : identity?.codegen === 'jit' ? runtimeEvidence?.isLoaded !== true : null;
  return {
    schemaVersion: 'tgfx-benchmark.ui-report.v8',
    testIntent,
    backend: options.backend, caseId: options.caseId, sceneName: options.sceneName,
    sceneVersion: summary.sceneVersion ?? frameWindow()?.__tgfxBenchmarkConfig?.sceneVersion ?? null,
    shaderGroups: options.caseId === 'coldstart.shader-mix'
      ? [...shaderMixGroups] : null,
    legacyOptions: options.caseId.startsWith('legacy.particles-') ? {
      antialias: options.antialias,
      stroke: options.stroke,
    } : null,
    tgfxPackage: options.tgfxPackage,
    tgfxMode: options.engineMode,
    tgfx: identity ? {
      packageId: identity.packageId,
      backend: identity.backend,
      codegen: identity.codegen,
      commit: identity.commit,
      dirty: identity.dirty,
      sourceMode: identity.sourceMode || null,
      sourceWorkspaceDirty: identity.sourceWorkspaceDirty ?? identity.dirty,
      sourceDiff: identity.sourceDiff || null,
      tag: identity.tag || null,
      compatibilityPatches: identity.compatibilityPatches || [],
      sourceName: identity.sourceName,
      builtAt: identity.builtAt,
      artifacts: identity.artifacts,
      benchmark: identity.benchmark,
      runtime: runtimeEvidence,
      runtimeValidity,
    } : null,
    count: coldStart ? finalCount : summary.adaptive ? (thresholdCount || analysisCount) : finalCount,
    mode: coldStart ? 'cold-start' : options.mode === 'ramp'
      ? 'ramp' : summary.manual ? 'manual' : 'fixed',
    executionStatus: summary.executionStatus, stopReason: summary.stopReason,
    warmupDurationMs: summary.warmupDurationUs / 1000,
    warmupFrameCount: summary.warmupFrameCount ?? 0,
    measurementDurationMs: summary.measurementDurationUs / 1000,
    sampleCount: analyzedSamples.length, overflowed: summary.overflowed,
    recordedFrameCount: summary.recordedFrameCount ?? summary.sampleCount,
    samplePolicy: summary.overflowed && summary.recordedFrameCount > summary.sampleCount
      ? 'latest-8192' : summary.overflowed ? 'first-8192' : 'all',
    analysisKind,
    analysisSampleCount: analyzedSamples.length,
    targetBudgetMs,
    overBudgetCount: coldStart ? 0 :
      intervals.filter(value => value > targetBudgetMs).length,
    platform: navigator.platform, userAgent: navigator.userAgent,
    runNonce: options.runNonce || null,
    cacheIsolation: 'context+assets',
    displayRefreshFps: frameWindow()?.__tgfxBenchmarkDisplayFps?.() ?? null,
    viewport: {
      width: frameWindow()?.document.querySelector('#benchmark')?.clientWidth || null,
      height: frameWindow()?.document.querySelector('#benchmark')?.clientHeight || null,
      pixelWidth: frameWindow()?.document.querySelector('#benchmark')?.width || null,
      pixelHeight: frameWindow()?.document.querySelector('#benchmark')?.height || null,
      devicePixelRatio: frameWindow()?.devicePixelRatio || window.devicePixelRatio,
    },
    firstSubmitMs: summary.firstSubmitUs / 1000,
    drainMs: summary.drainAfterMeasurementUs / 1000,
    intervalP50Ms: quantile(intervals, .5),
    intervalP95Ms: quantile(intervals, .95),
    intervalP99Ms: quantile(intervals, .99),
    intervalMaxMs: intervals.length ? Math.max(...intervals) : null,
    slowFrameCount: intervals.filter(value => value > 33.3).length,
    measuredFps: intervals.length ? 1000 * intervals.length /
      intervals.reduce((sum, interval) => sum + interval, 0) : null,
    rawSamples: samples,
    intervals,
    phases: {
      draw: phase(analyzedSamples, 'sceneDrawUs'),
      flush: phase(analyzedSamples, 'flushUs'),
      submit: phase(analyzedSamples, 'submitUs'),
      callback: {
        p50: quantile(analyzedSamples.map(s =>
          (s.callbackEndUs - s.callbackBeginUs) / 1000), .5),
        p95: quantile(analyzedSamples.map(s =>
          (s.callbackEndUs - s.callbackBeginUs) / 1000), .95),
      },
    },
    countTimeline: summary.adaptive ? buildCountTimeline(samples) : [],
    coldStart: coldStart ? {
      initialCount: finalCount,
      firstSubmitMs: summary.firstSubmitUs / 1000,
      firstVisibleMs: summary.coldStartFirstVisibleUs / 1000,
      firstFrame,
      firstFrameCallbackMs: firstFrame.callbackUs === null
        ? null : firstFrame.callbackUs / 1000,
      firstFrameSceneDrawMs: firstFrame.sceneDrawUs === null
        ? null : firstFrame.sceneDrawUs / 1000,
      firstFrameFlushMs: firstFrame.flushUs === null ? null : firstFrame.flushUs / 1000,
      firstFrameSubmitUs: firstFrame.submitUs === null ? null : firstFrame.submitUs / 1000,
      intervalP50Ms: quantile(intervals, .5),
      intervalP95Ms: quantile(intervals, .95),
      intervalMaxMs: intervals.length ? Math.max(...intervals) : null,
      initialWindowFrames: summary.coldStartFramesAfterVisible ?? 0,
    } : null,
    adaptive: summary.adaptive ? {
      targetFps: summary.adaptiveTargetFps,
      toleranceFps: Math.max(0.5, summary.adaptiveTargetFps * 0.01),
      stableWindowMs: summary.adaptiveStableWindowUs / 1000,
      initialCount: summary.adaptiveInitialCount,
      peakCount: summary.adaptivePeakCount,
      finalCount,
      analysisCount,
      lowPassingCount: summary.adaptiveLowPassingCount,
      highFailingCount: summary.adaptiveHighFailingCount,
      lowPassingFps: summary.adaptiveLowPassingFps,
      highFailingFps: summary.adaptiveHighFailingFps,
      searchIteration: summary.adaptiveSearchIteration ?? 0,
      settledCount: summary.adaptiveSettledCount,
      settled: summary.adaptiveSettled,
      targetReached: summary.adaptiveTargetReached,
      thresholdReached: summary.adaptiveThresholdReached,
      capacityLimited: summary.adaptiveCapacityLimited,
      retractionCount: summary.adaptiveRetractionCount,
      stabilityLossCount: summary.adaptiveStabilityLossCount ?? 0,
      evaluatedWindowCount: summary.adaptiveWindowCount ?? 0,
      lastWindowFps: summary.adaptiveWindowFps ?? null,
      lastWindowP95Ms: summary.adaptiveWindowP95Us / 1000,
      p95AllowanceMs: Math.max(1, (1000 / summary.adaptiveTargetFps) * 0.05),
      windowPolicy: '30-interval-search;60-interval-confirmation;100ms-after-count-change;severe-fail-fast-8-intervals-1s',
      rampDurationMs: summary.adaptiveRampDurationUs / 1000,
      stableStartMs: stableSamples.length
        ? (stableSamples[0].callbackBeginUs - samples[0].callbackBeginUs) / 1000 : null,
      stableDurationMs: stableSamples.length >= 2
        ? (stableSamples[stableSamples.length - 1].callbackBeginUs -
          stableSamples[0].callbackBeginUs) / 1000 : null,
    } : null,
  };
}

function aotFailureCount(runtime) {
  if (!runtime || runtime.available !== true) return 0;
  const counts = runtime.fallbackCounts;
  if (!counts || typeof counts !== 'object') return Number(runtime.failureCount || 0);
  return Object.entries(counts).reduce((total, [reason, count]) =>
    reason === 'DeferredTexture' ? total : total + Number(count || 0), 0);
}

function reportWarnings(report) {
  const warnings = [];
  if (!report.tgfx) {
    warnings.push('报告缺少 TGFX 包身份，无法确认被测提交。');
  } else if (report.tgfx.codegen === 'aot') {
    if (!report.tgfx.runtime?.available || !report.tgfx.runtime?.isLoaded) {
      warnings.push('AOT 包没有成功加载预编译 bundle，本次结果不能标记为 AOT。');
    } else if (aotFailureCount(report.tgfx.runtime) > 0) {
      const failureCount = aotFailureCount(report.tgfx.runtime);
      const details = Object.entries(report.tgfx.runtime.fallbackCounts || {})
        .filter(([name]) => name !== 'DeferredTexture')
        .map(([name, count]) => `${name} ${count}`).join('、');
      warnings.push(
        `AOT 运行期记录了 ${failureCount} 次预编译查找/接口失败` +
        `${details ? `（${details}）` : ''}。部分 draw 可能已被丢弃，本次结果不能作为有效的 AOT 对比。`,
      );
    }
  } else if (report.tgfx.runtime?.isLoaded) {
    warnings.push('JIT/main 包意外加载了 AOT bundle，请检查构建产物。');
  }
  if (report.overflowed && !report.adaptive) {
    warnings.push(report.samplePolicy === 'latest-8192'
      ? '采样缓冲区已循环覆盖，统计仅包含最近 8192 帧。'
      : '采样缓冲区已满，统计仅包含开始保留的帧。');
  }
  if (report.testIntent === 'cold-start') {
    if ((report.coldStart?.initialWindowFrames || 0) < 30) {
      warnings.push('首帧后辅助窗口不足 30 帧，初始阶段统计不完整。');
    }
  } else if (report.adaptive && !report.adaptive.targetReached) {
    if (report.adaptive.thresholdReached) {
      // A quick-ramp threshold is a valid estimated result, not a failed run.
    } else if (report.adaptive.capacityLimited) {
      warnings.push('场景已到达当前实现的容量边界，没有生成稳定区间。');
    } else if (report.stopReason !== 'adaptive-unsettled-user-stop') {
      warnings.push('自适应搜索异常结束，没有生成稳定区间。');
    }
  } else if ((report.sampleCount || 0) < 2) {
    warnings.push('稳定区间连续帧不足，无法统计帧间隔。');
  } else if ((report.sampleCount || 0) < 30) {
    warnings.push(`稳定区间仅保留 ${report.sampleCount} 帧，帧间隔统计置信度较低。`);
  }
  return warnings;
}

function rampVerdict(report) {
  const adaptive = report.adaptive;
  if (!adaptive) return '没有记录自适应爬坡。';
  const lastWindow = Number.isFinite(adaptive.lastWindowFps)
    ? `最后一次判定为 ${adaptive.lastWindowFps.toFixed(1)} FPS、p95 ` +
      `${formatMs(adaptive.lastWindowP95Ms)}。`
    : '尚无完整的判定窗口。';
  if (adaptive.thresholdReached) {
    return `快速爬坡在 ${adaptive.settledCount} 个对象处连续两个同数量窗口未满足目标；` +
      `${lastWindow}报告使用的是该触边对象数下的最后一个观测窗口，代表估算容量，` +
      '不等同于经过长时间确认的稳定容量。';
  }
  if (report.stopReason === 'adaptive-unsettled-user-stop' && !adaptive.capacityLimited) {
    return `在自适应搜索完成前手动结束。分析窗口对象数为 ${adaptive.analysisCount ?? report.count}；` +
      `${lastWindow}这组数据表示当前负载表现，不作为稳定容量结论。`;
  }
  if (adaptive.capacityLimited) {
    return `场景实际对象数停在 ${adaptive.settledCount}，继续增加不会产生实际绘制工作；` +
      `${lastWindow}` +
      `因此记录为场景容量边界，而不是目标 ${adaptive.targetFps} FPS 的稳定结果。`;
  }
  if (!adaptive.targetReached) {
    return `本次运行没有通过目标 ${adaptive.targetFps} FPS 的连续稳定判定或结束复核。` +
      `结束时实际对象数为 ${adaptive.finalCount ?? report.count}；报告不把它记为稳定对象。` +
      `${lastWindow}爬坡和失稳样本保留在原始数据中。`;
  }
  return `从 ${adaptive.initialCount} 个对象开始连续增长，最高尝试 ${adaptive.peakCount} 个；` +
    `经过 ${adaptive.retractionCount ?? 0} 次回撤后，在 ${adaptive.settledCount} 个对象、` +
    `通过区间 ${adaptive.lowPassingCount ?? '-'}、失败区间 ${adaptive.highFailingCount ?? '-'} 收敛，` +
    `FPS 不低于 ${(adaptive.targetFps - adaptive.toleranceFps).toFixed(1)}，` +
    `p95 不超过 ${formatMs(report.targetBudgetMs + adaptive.p95AllowanceMs)} 的条件下连续稳定。` +
    `没有设置对象数上限，稳定判定窗口为 ${formatDuration(adaptive.stableWindowMs)}。`;
}

function drawRampChart() {
  const chart = $('ramp-chart');
  const timeline = state.report?.countTimeline;
  const adaptive = state.report?.adaptive;
  if (state.view !== 'report' || !timeline?.length || !adaptive) return;
  const rect = chart.getBoundingClientRect();
  if (!rect.width || !rect.height) return;
  const dpr = Math.min(2, devicePixelRatio || 1);
  chart.width = Math.round(rect.width * dpr);
  chart.height = Math.round(rect.height * dpr);
  const ctx = chart.getContext('2d');
  ctx.scale(dpr, dpr);
  const w = rect.width, h = rect.height, pad = 42;
  const maxCount = Math.max(adaptive.peakCount || 1, ...timeline.map(point => point.count));
  const maxTime = Math.max(1, timeline[timeline.length - 1].elapsedMs);
  const y = value => h - pad - Math.min(value, maxCount) / maxCount * (h - pad - 14);
  const x = value => pad + Math.min(value, maxTime) / maxTime * (w - pad - 12);
  ctx.strokeStyle = '#454a51';
  ctx.beginPath(); ctx.moveTo(pad, h - pad); ctx.lineTo(w - 10, h - pad); ctx.stroke();
  ctx.setLineDash([4, 4]); ctx.strokeStyle = '#9bc5ff';
  ctx.beginPath();
  ctx.moveTo(pad, y(adaptive.settledCount));
  ctx.lineTo(w - 10, y(adaptive.settledCount));
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.fillStyle = '#abafb7'; ctx.font = '11px sans-serif';
  ctx.fillText(String(maxCount), Math.max(pad, w - 64), 16);
  ctx.fillText('0', 22, h - pad + 4);
  ctx.fillText(`${Math.round(maxTime)} ms`, Math.max(pad, w - 66), h - 12);
  ctx.strokeStyle = '#6bd69a'; ctx.lineWidth = 2; ctx.beginPath();
  timeline.forEach((point, index) => {
    if (index === 0) ctx.moveTo(x(point.elapsedMs), y(point.count));
    else ctx.lineTo(x(point.elapsedMs), y(point.count));
  });
  ctx.stroke();
  ctx.fillStyle = '#9bc5ff';
  ctx.beginPath();
  ctx.arc(x(maxTime), y(adaptive.settledCount), 4, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = '#abafb7';
  ctx.fillText('对象数', 2, 16);
}

function drawChart() {
  if (state.report?.testIntent === 'cold-start') {
    drawColdFrameChart();
    return;
  }
  const chart = $('interval-chart');
  if (state.view !== 'report' || !state.report) return;
  drawRampChart();
  const rect = chart.getBoundingClientRect();
  if (!rect.width || !rect.height) return;
  const dpr = Math.min(2, devicePixelRatio || 1);
  chart.width = Math.round(rect.width * dpr);
  chart.height = Math.round(rect.height * dpr);
  const ctx = chart.getContext('2d');
  ctx.scale(dpr, dpr);
  const width = rect.width, height = rect.height, pad = 32;
  const values = state.report.intervals || [];
  const budget = state.report.targetBudgetMs || 1000 / 60;
  const ceiling = Math.max(33.3, budget, Math.ceil((state.report.intervalMaxMs || 33.3) / 10) * 10);
  ctx.strokeStyle = '#454a51';
  ctx.beginPath(); ctx.moveTo(pad, height - pad); ctx.lineTo(width - 8, height - pad); ctx.stroke();
  const y = ms => height - pad - Math.min(ms, ceiling) / ceiling * (height - pad - 12);
  ctx.setLineDash([4, 4]); ctx.strokeStyle = '#f0bf66';
  ctx.beginPath(); ctx.moveTo(pad, y(budget)); ctx.lineTo(width - 8, y(budget)); ctx.stroke();
  ctx.setLineDash([]);
  ctx.fillStyle = '#abafb7'; ctx.font = '11px sans-serif';
  ctx.fillText(budget.toFixed(1), 2, y(budget) + 4);
  ctx.fillText(`${ceiling} ms`, 2, 16);
  if (!values.length) {
    ctx.fillText('连续采样帧数不足', pad + 12, height / 2);
    return;
  }
  ctx.strokeStyle = '#6bd69a'; ctx.lineWidth = 1.5;
  ctx.beginPath();
  values.forEach((value, index) => {
    const x = pad + index / Math.max(1, values.length - 1) * (width - pad - 8);
    if (!index) ctx.moveTo(x, y(value));
    else ctx.lineTo(x, y(value));
  });
  ctx.stroke();
}

function drawColdFrameChart() {
  const chart = $('cold-frame-chart');
  const samples = state.report?.rawSamples || [];
  if (state.view !== 'report' || !samples.length) return;
  const rect = chart.getBoundingClientRect();
  if (!rect.width || !rect.height) return;
  const dpr = Math.min(2, devicePixelRatio || 1);
  chart.width = Math.round(rect.width * dpr);
  chart.height = Math.round(rect.height * dpr);
  const ctx = chart.getContext('2d');
  ctx.scale(dpr, dpr);
  const width = rect.width;
  const height = rect.height;
  const pad = 34;
  const values = samples.map((sample, index) => index === 0
    ? (sample.callbackEndUs - sample.callbackBeginUs) / 1000
    : (sample.callbackBeginUs - samples[index - 1].callbackBeginUs) / 1000);
  const ceiling = Math.max(33.3, Math.ceil(Math.max(...values) / 10) * 10);
  const y = value => height - pad -
    Math.min(value, ceiling) / ceiling * (height - pad - 12);
  ctx.strokeStyle = '#454a51';
  ctx.beginPath();
  ctx.moveTo(pad, height - pad);
  ctx.lineTo(width - 8, height - pad);
  ctx.stroke();
  ctx.strokeStyle = '#6bd69a';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  values.forEach((value, index) => {
    const x = pad + index / Math.max(1, values.length - 1) * (width - pad - 8);
    if (!index) ctx.moveTo(x, y(value));
    else ctx.lineTo(x, y(value));
  });
  ctx.stroke();
  ctx.fillStyle = '#f0bf66';
  ctx.beginPath();
  ctx.arc(pad, y(values[0]), 4, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = '#abafb7';
  ctx.font = '11px sans-serif';
  ctx.fillText('耗时 / 帧间隔（毫秒，ms）', 2, 14);
  ctx.fillText(`${ceiling} ms`, 2, 28);
  ctx.fillText('0 ms', pad - 8, height - 8);
  ctx.fillText('帧 1', pad - 3, height - 20);
  ctx.fillText(`帧 ${values.length}`, width - 48, height - 20);
  ctx.fillText('帧序号', width - 42, height - 8);
  ctx.fillText('首帧 CPU 回调', pad + 7, Math.max(44, y(values[0]) - 8));
}

function tgfxIdentityLabel(identity, compact = true, includeMode = true) {
  if (!identity) return 'TGFX 未知';
  const commit = compact ? shortCommit(identity.commit) : identity.commit;
  const label = `TGFX ${commit}${identity.dirty ? ' +dirty' : ''}`;
  return includeMode ? `${label} · ${engineLabel(identity.codegen)}` : label;
}

const steadySessionColumns = [
  ['tgfx', 'TGFX'], ['mode', '执行路径'], ['backend', '后端'], ['scene', '场景'],
  ['count', '对象数'], ['status', '状态'], ['firstSubmit', '首次 submit'],
  ['warmup', '启动隔离'], ['callbackP95', 'CPU callback p95'],
  ['p95', '帧间隔 p95'], ['fps', '平均 FPS'], ['runtime', 'AOT 证据'],
  ['samples', '采样帧数'],
];

const coldSessionColumns = [
  ['tgfx', 'TGFX'], ['mode', '执行路径'], ['backend', '后端'], ['scene', '场景'],
  ['count', '初始绘制数'],
  ['firstSubmit', '首次命令提交'], ['firstVisible', '首帧后下一次刷新回调'],
  ['firstCallback', '首帧 CPU 回调'], ['windowP95', '后续 30 帧 p95（ms）'],
  ['runtime', 'AOT 证据'],
];

function runtimeEvidenceLabel(run) {
  if (run.tgfx?.codegen !== 'aot') return 'JIT';
  if (run.tgfx?.runtime?.isLoaded !== true) return '未加载';
  const hits = run.tgfx.runtime.hitCount;
  const misses = run.tgfx.runtime.missCount;
  const failures = run.tgfx.runtime.failureCount;
  const families = run.tgfx.runtime.hitSummary?.length;
  if (Number.isFinite(hits)) {
    const counts = `hit ${hits} / miss ${Number.isFinite(misses) ? misses : '-'}`;
    const familyLabel = families ? ` · ${families} 类` : '';
    const failureLabel = failures ? ` / fail ${failures}` : '';
    const prefix = run.tgfx.runtimeValidity === false ? '不完整 · ' : '';
    return `${prefix}${counts}${familyLabel}${failureLabel}`;
  }
  return run.tgfx.runtimeValidity === false ? '不完整' : '已加载';
}

function renderSessionHistory(coldStart) {
  const columns = coldStart ? coldSessionColumns : steadySessionColumns;
  $('session-history-title').textContent =
    coldStart ? '首帧测试会话记录' : '稳态测试会话记录';
  $('recent-runs-head').replaceChildren(...columns.map(([, label]) => {
    const header = document.createElement('th');
    header.textContent = label;
    return header;
  }));
  const intent = coldStart ? 'cold-start' : 'steady-state';
  const runs = state.history.filter(run =>
    (run.testIntent === 'cold-start' ? 'cold-start' : 'steady-state') === intent).slice(0, 8);
  $('recent-runs').replaceChildren(...runs.map(run => {
    const row = document.createElement('tr');
    const values = coldStart ? {
      tgfx: tgfxIdentityLabel(run.tgfx, true, false),
      mode: engineLabel(run.tgfx?.codegen),
      backend: run.backend,
      scene: run.sceneName || run.caseId,
      count: run.count,
      firstSubmit: formatMs(run.coldStart?.firstSubmitMs),
      firstVisible: formatMs(run.coldStart?.firstVisibleMs),
      firstCallback: formatMs(run.coldStart?.firstFrameCallbackMs),
      windowP95: formatMs(run.coldStart?.intervalP95Ms),
      runtime: runtimeEvidenceLabel(run),
    } : {
      tgfx: tgfxIdentityLabel(run.tgfx, true, false),
      mode: engineLabel(run.tgfx?.codegen),
      backend: run.backend,
      scene: run.sceneName || run.caseId,
      count: run.count,
      status: run.tgfx?.runtimeValidity === false ? 'AOT 不完整'
        : run.adaptive?.thresholdReached ? '估算触边'
          : run.adaptive?.targetReached ? '已稳定'
            : run.adaptive?.capacityLimited ? '容量边界' : '已结束',
      firstSubmit: formatMs(run.firstSubmitMs),
      warmup: `${run.warmupFrameCount ?? 0} 帧`,
      callbackP95: formatMs(run.phases?.callback?.p95),
      p95: formatMs(run.intervalP95Ms),
      fps: Number.isFinite(run.measuredFps) ? run.measuredFps.toFixed(1) : '-',
      runtime: runtimeEvidenceLabel(run),
      samples: run.sampleCount,
    };
    for (const [key] of columns) {
      const cell = document.createElement('td');
      cell.dataset.column = key;
      if (key === 'mode') {
        cell.classList.add('engine-mode-cell',
          run.tgfx?.codegen === 'aot' ? 'engine-aot' : 'engine-jit');
      }
      cell.textContent = String(values[key] ?? '-');
      row.appendChild(cell);
    }
    return row;
  }));
}

function renderReport() {
  const report = state.report;
  if (!report) return;
  const coldStart = report.testIntent === 'cold-start';
  const modeLabel = coldStart ? '首帧测试' : report.mode === 'ramp'
    ? report.adaptive?.targetReached ? '自适应爬坡 / 稳定观察'
      : report.adaptive?.thresholdReached ? '快速爬坡 / 估算触边' : '快速爬坡 / 当前结果'
    : report.mode === 'fixed' ? '固定时长' : '手动观察';
  const methodLabel = coldStart ? 'TGFX 首次渲染' :
    report.mode === 'fixed' ? '固定时长测量' : modeLabel;
  $('report-empty').classList.add('hidden');
  $('report-content').classList.remove('hidden');
  $('report-title').textContent = coldStart ? '首帧测试报告' : '稳态测量报告';
  $('download').disabled = false;
  $('report-subtitle').textContent =
    `${report.sceneName || report.caseId} · ${tgfxIdentityLabel(report.tgfx)} · ${backendLabel(report.backend)} · ${report.count} ${countUnit(report.caseId)} · ${modeLabel}`;
  $('report-subtitle').title = tgfxIdentityLabel(report.tgfx, false);
  $('steady-report').classList.toggle('hidden', coldStart);
  $('cold-report').classList.toggle('hidden', !coldStart);
  if (coldStart) {
    const cold = report.coldStart || {};
    $('cold-count-label').textContent =
      report.caseId === 'coldstart.shader-mix' ? '初始绘制单元' : '初始对象数';
    for (const [id, value] of [
      ['cold-first-submit', formatMs(cold.firstSubmitMs)],
      ['cold-first-visible', formatMs(cold.firstVisibleMs)],
      ['cold-first-callback', formatMs(cold.firstFrameCallbackMs)],
      ['cold-count', String(cold.initialCount ?? report.count)],
      ['cold-p50', formatMs(cold.intervalP50Ms)],
      ['cold-p95', formatMs(cold.intervalP95Ms)],
      ['cold-max', formatMs(cold.intervalMaxMs)],
      ['cold-samples', `${cold.initialWindowFrames ?? '-'} 帧`],
    ]) $(id).textContent = value;
    $('cold-shader-groups').textContent = Array.isArray(report.shaderGroups) &&
      report.shaderGroups.length > 0
      ? report.shaderGroups
        .map(group => shaderMixGroupLabels[group] || group).join(' · ') : '-';
    $('cold-note').textContent =
      `横轴是帧序号：第 1 帧为首帧，后面是首帧完成后的 ${cold.initialWindowFrames || 0} 帧。` +
      '纵轴单位是毫秒（ms）：第 1 个点表示首帧 CPU 回调耗时，后续点表示相邻两帧回调的时间间隔。' +
      '“下一次刷新回调”是浏览器 requestAnimationFrame 回调，不等于像素已经显示到屏幕，也不等于 GPU 执行时间。' +
      '这段数据只用于观察启动成本，不代表稳态容量。';
  } else {
    for (const [id, value] of [
      ['metric-p50', formatMs(report.intervalP50Ms)],
      ['metric-fps', Number.isFinite(report.measuredFps) ? report.measuredFps.toFixed(1) : '-'],
      ['metric-p95', formatMs(report.intervalP95Ms)],
      ['metric-p99', formatMs(report.intervalP99Ms)],
      ['metric-max', formatMs(report.intervalMaxMs)],
      ['metric-duration', formatDuration(report.adaptive?.stableDurationMs ??
        report.measurementDurationMs)],
      ['metric-samples', String(report.sampleCount ?? '-')],
      ['metric-slow', String(report.slowFrameCount ?? '-')],
      ['metric-over-budget', Number.isFinite(report.targetBudgetMs) && report.intervals?.length
        ? `${(100 * (report.overBudgetCount ?? report.intervals.filter(v => v > report.targetBudgetMs).length) / report.intervals.length).toFixed(1)}%`
        : '-'],
    ]) $(id).textContent = value;
    for (const [id, metric] of Object.entries({
      'phase-draw': report.phases?.draw, 'phase-flush': report.phases?.flush,
      'phase-submit': report.phases?.submit, 'phase-callback': report.phases?.callback,
    })) $(id).textContent = metric ? `${formatMs(metric.p50)} / ${formatMs(metric.p95)}` : '-';
  }

  const warnings = reportWarnings(report);
  $('report-warning').textContent = warnings.join(' ');
  $('report-warning').classList.toggle('hidden', !warnings.length);
  $('ramp-results').classList.toggle('hidden', coldStart || !report.adaptive);
  if (!coldStart && report.adaptive) {
    $('ramp-initial').textContent = String(report.adaptive.initialCount);
    $('ramp-peak').textContent = String(report.adaptive.peakCount);
    $('ramp-final').textContent = String(report.adaptive.finalCount ?? report.count);
    $('ramp-settled').textContent = String(report.adaptive.settledCount);
    $('ramp-target').textContent =
      `≥ ${(report.adaptive.targetFps - report.adaptive.toleranceFps).toFixed(1)} FPS`;
    $('ramp-last-fps').textContent = Number.isFinite(report.adaptive.lastWindowFps)
      ? `${report.adaptive.lastWindowFps.toFixed(1)} FPS` : '-';
    $('ramp-last-p95').textContent = formatMs(report.adaptive.lastWindowP95Ms);
    $('ramp-time').textContent = formatDuration(report.adaptive.rampDurationMs);
    $('ramp-window').textContent = report.adaptive.thresholdReached
      ? '未使用' : formatDuration(report.adaptive.stableWindowMs);
    $('ramp-retractions').textContent = String(report.adaptive.retractionCount ?? '-');
    const analysisNote = report.adaptive.targetReached
      ? '上方帧间隔指标仅统计已验证稳定区间。'
      : report.adaptive.thresholdReached
        ? '上方帧间隔指标取自触边对象数下的最后一个同数量窗口。'
        : '上方帧间隔指标取自最后一段同对象数观测窗口。';
    $('ramp-verdict').textContent = rampVerdict(report) + ' ' + analysisNote +
      ' 完整爬坡过程保留在对象数量轨迹和原始样本中。';
  }
  const packageDetails = report.tgfx
    ? `包 ${report.tgfx.packageId}；源码 ${report.tgfx.sourceName || 'unknown'} @ ${report.tgfx.commit || 'unknown'}；`
    : '';
  $('report-description').textContent = coldStart
    ? `${tgfxIdentityLabel(report.tgfx)} · ${report.backend} · ${methodLabel}。` +
      packageDetails +
      `首次渲染前已设置 ${report.count} ${countUnit(report.caseId)}，不使用预热或爬坡。` +
      `首次命令提交（自测试开始计时）${formatMs(report.coldStart?.firstSubmitMs)}，` +
      `首帧后下一次刷新回调（同样自测试开始计时）${formatMs(report.coldStart?.firstVisibleMs)}；` +
      `随后连续采集 ${report.coldStart?.initialWindowFrames || 0} 个首帧后渲染回调。` +
      '绘制、Flush 与 Submit 均为 CPU 侧耗时；后续帧只用于观察启动阶段，不作为稳态容量结论。'
    : `${tgfxIdentityLabel(report.tgfx)} · ${report.backend} · ${methodLabel}。` +
      packageDetails +
      `启动隔离 ${report.warmupFrameCount || 0} 帧不计入测量；实际启动阶段 ${formatDuration(report.warmupDurationMs)}。` +
      `共记录 ${report.recordedFrameCount ?? report.sampleCount} 帧；分析窗口保留 ${report.sampleCount} 帧，统计 ${report.intervals?.length ?? Math.max(0, (report.sampleCount || 0) - 1)} 个帧间隔。` +
      (report.samplePolicy === 'latest-8192'
        ? ' 原始样本缓冲区仅保留最近 8192 帧；本报告指标取自最后的同对象数窗口，不受更早样本覆盖影响。'
        : '') +
      `首次提交 ${formatMs(report.firstSubmitMs)}，收尾提交 ${formatMs(report.drainMs)}。` +
      '绘制、Flush 与 Submit 均为 CPU 侧耗时，渲染提交上一帧的录制内容。' +
      (report.adaptive
        ? report.adaptive.thresholdReached
          ? '快速爬坡使用 30 帧搜索窗口；连续两个同对象数窗口失败后冻结数量，报告的是估算触边结果。'
          : report.adaptive.targetReached
            ? '初步搜索使用同对象数、互不重叠的 30 个帧间隔；边界确认与稳态观察使用 60 个帧间隔。变更对象数后排除 100 ms 过渡期。' +
              'FPS 与 p95 同时达标才累计连续稳定时长；任一窗口失败就清零，两次连续失败才回撤。严重过载时可用至少 8 个间隔、至少 1 秒的窗口提前确认失败，不能提前判定通过。' +
              '控制器维护通过/失败上下界，并用实测 FPS 与 p95 成本估算下一次对象数，区间收敛后才判定稳定。' +
              '稳定观察中持续复查；此结果是满足目标下限的已验证负载，不是精确最大容量。'
            : '快速爬坡在完成触边确认前结束；帧间隔指标来自最后一段同对象数观测窗口。'
        : '');
  $('interval-note').textContent =
    `帧间隔按相邻回调起点计算；虚线为目标预算 ${formatMs(report.targetBudgetMs || 1000 / 60)}，不是 GPU 执行时间。` +
    `${report.platform || '未知平台'} · ${report.viewport?.pixelWidth ?? report.viewport?.width ?? '-'} × ${report.viewport?.pixelHeight ?? report.viewport?.height ?? '-'} 渲染像素` +
    `${report.viewport?.devicePixelRatio ? ` · DPR ${report.viewport.devicePixelRatio}` : ''}` +
    `${report.displayRefreshFps
      ? ` · 负载下浏览器刷新上限 ${report.displayRefreshFps.toFixed(0)} 次/秒`
      : ''}。`;
  renderSessionHistory(coldStart);
  drawChart();
}

function capturePreview() {
  try {
    const dataUrl = frameWindow()?.__tgfxBenchmark?.completionPreviewDataUrl;
    if (typeof dataUrl !== 'string' || !dataUrl.startsWith('data:image/')) {
      throw new Error('Completion preview is unavailable');
    }
    $('report-preview').src = dataUrl;
    $('report-preview').classList.remove('hidden');
  } catch (error) {
    console.warn('Frame capture unavailable', error);
    $('report-preview').removeAttribute('src');
    $('report-preview').classList.add('hidden');
  }
}

function endRun(manualStop = false) {
  if (!state.running) return;
  if (manualStop) frameWindow()?.__tgfxBenchmarkView?.stopBenchmarkRun();
  const summary = frameWindow()?.__tgfxBenchmark?.benchmarkSummary;
  if (!summary || summary.executionStatus !== 'completed') {
    showError('没有已完成的测量，请检查场景并重试。');
    return;
  }
  clearInterval(state.timer);
  state.timer = null;
  capturePreview();
  state.report = summarize(summary, state.config);
  state.history.unshift(state.report);
  state.running = false;
  controls();
  renderReport();
  $('scene-phase').textContent = '已完成';
  $('scene-message').textContent = '测量已完成';
  setView('report');
}

function poll(generation) {
  if (!state.running || generation !== state.generation) return;
  try {
    if (new URLSearchParams(frameWindow()?.location.search).get('runNonce') !==
        state.config.runNonce) return;
    const stats = sampleStats();
    const initializationError = frameWindow()?.__tgfxBenchmark?.initializationError;
    if (initializationError) {
      cancelRun(`场景初始化失败：${initializationError}`);
      return;
    }
    if (!stats) {
      if (performance.now() - state.startedAt > 20000) cancelRun('场景在 20 秒内未能完成初始化。');
      return;
    }
    state.adaptive = stats.adaptive ? {
      quick: stats.adaptiveQuick,
      settled: stats.adaptiveSettled,
      targetReached: stats.adaptiveTargetReached,
      thresholdReached: stats.adaptiveThresholdReached,
      capacityLimited: stats.adaptiveCapacityLimited,
      stabilizing: stats.adaptiveStabilizing,
      initialCount: stats.adaptiveInitialCount,
      peakCount: stats.adaptivePeakCount,
      settledCount: stats.adaptiveSettledCount,
      targetFps: stats.adaptiveTargetFps,
    } : null;
    const progressFrame = stats.drawnFrameCount ?? stats.recordedFrameCount;
    if (progressFrame !== state.lastProgressFrame) {
      state.lastProgressFrame = progressFrame;
      state.lastProgressAt = performance.now();
    } else if (performance.now() - state.lastProgressAt > 10000) {
      cancelRun('渲染回调已停止超过 10 秒，测量无效。请降低初始负载并检查浏览器错误。');
      return;
    }
    const phaseName = stats.complete ? '已完成' :
      state.config.testIntent === 'cold-start'
        ? stats.coldStartFirstVisibleUs > 0
          ? `首帧后采样 ${stats.coldStartFramesAfterVisible} / ` +
            `${stats.coldStartInitialWindowFrames} 帧`
          : '首帧渲染中'
        : state.runMode === 'ramp'
        ? stats.adaptiveRampElapsedUs <= 0 ? '预热中'
          : stats.adaptiveThresholdReached ? '估算触边'
            : stats.adaptiveSettled ? stats.adaptiveTargetReached ? '稳定观察中'
              : stats.adaptiveCapacityLimited ? '场景容量受限' : '目标不可达'
            : stats.adaptiveStabilizing ? '稳定判定中' : '自动爬坡中'
        : !stats.measurementStarted
          ? `启动隔离 ${stats.successfulFrameCount}/${stats.warmupFrameCount || 10}`
          : '测量中';
    $('live-phase').textContent = phaseName;
    $('status-state').textContent = phaseName;
    $('scene-phase').textContent = phaseName;
    $('live-fps').textContent = stats.currentFps > 0 ? stats.currentFps.toFixed(1) : '-';
    $('live-refresh').textContent = stats.displayRefreshFps
      ? `${stats.displayRefreshFps.toFixed(0)} 次/秒` : '-';
    $('live-count').textContent = String(stats.sceneCount || state.config.count);
    updateSceneCount(stats.sceneCount || state.config.count);
    $('live-first-submit').textContent =
      stats.firstSubmitUs > 0 ? formatMs(stats.firstSubmitUs / 1000) : '-';
    $('live-first-visible').textContent = stats.coldStartFirstVisibleUs > 0
      ? formatMs(stats.coldStartFirstVisibleUs / 1000) : '-';
    $('live-initial-window').textContent =
      `${stats.coldStartFramesAfterVisible || 0} / ` +
      `${stats.coldStartInitialWindowFrames || 30} 帧`;
    $('live-peak').textContent = stats.adaptive ? String(stats.adaptivePeakCount) : '-';
    $('live-settled').textContent = stats.adaptiveSettledCount
      ? String(stats.adaptiveSettledCount) : '-';
    $('live-search-range').textContent = stats.adaptive
      ? `${stats.adaptiveLowPassingCount || '-'} … ${stats.adaptiveHighFailingCount || '-'}`
      : '-';
    $('live-elapsed').textContent = formatDuration(stats.elapsedUs / 1000);
    $('live-samples').textContent = String(stats.sampleCount);
    $('live-interval').textContent = formatMs(stats.lastIntervalMs);
    $('live-draw').textContent = formatMs(stats.lastSceneDrawMs);
    $('live-overflow').textContent = stats.overflowed ? '已覆盖' : '正常';
    $('scene-message').textContent =
      `${phaseName} · 已采样 ${stats.sampleCount} 帧 · 最近间隔 ${formatMs(stats.lastIntervalMs)}` +
      (stats.adaptiveWindowCount > 0
        ? ` · 判定 FPS ${stats.adaptiveWindowFps.toFixed(1)} / p95 ${formatMs(stats.adaptiveWindowP95Us / 1000)}`
        : '');
    updateHud(stats);
    updateResolution();
    ui.stop.disabled = !stats.active || stats.sampleCount < 2;
    if (state.config.testIntent === 'steady-state') {
      ui.stop.textContent = stats.adaptiveThresholdReached
        ? '结束并查看触边报告'
        : stats.adaptiveSettled ? '结束并查看报告' : '结束爬坡并查看当前结果';
    }
    if (stats.complete &&
        (state.config.testIntent === 'cold-start' ||
         (state.config.testIntent === 'steady-state' && state.runMode === 'fixed'))) {
      endRun();
    }
  } catch (error) {
    cancelRun(`场景出错：${error.message}`);
  }
}

async function startRun() {
  if (state.testIntent === 'steady-state' && state.runMode === 'ramp') {
    ui.count.removeAttribute('max');
  } else {
    ui.count.max = String(limits[ui.scene.value] || 2048);
  }
  if (state.running || state.starting || !validate()) return;
  state.starting = true;
  controls();
  const identity = await updatePackageIdentity();
  state.starting = false;
  controls();
  if (!identity) {
    showError('所选 TGFX 包未构建或身份无效，无法开始测量。');
    return;
  }
  state.config = {
    ...config(),
    runNonce: typeof crypto.randomUUID === 'function'
      ? crypto.randomUUID() : `${Date.now()}-${Math.random().toString(16).slice(2)}`,
  };
  if (state.testIntent === 'steady-state' && state.runMode === 'ramp') {
    const refreshFps = frameWindow()?.__tgfxBenchmarkUnloadedDisplayFps?.();
    if (refreshFps && state.config.targetFps > refreshFps * 1.05) {
      showError(
        `目标 ${state.config.targetFps} FPS 高于空载时浏览器刷新上限 ` +
        `${refreshFps.toFixed(0)} 次/秒，` +
        '该目标无法稳定达到。',
      );
      ui.targetFps.focus();
      return;
    }
  }
  state.adaptive = null;
  $('live-peak').textContent = String(state.config.count);
  $('live-settled').textContent = '-';
  $('live-budget').textContent = state.testIntent === 'cold-start'
    ? '首帧'
    : formatMs(1000 / (state.runMode === 'ramp' ? state.config.targetFps : 60));
  state.running = true;
  state.startedAt = performance.now();
  state.lastProgressFrame = null;
  state.lastProgressAt = state.startedAt;
  const generation = ++state.generation;
  $('live-phase').textContent = '加载中';
  $('live-samples').textContent = '0';
  $('live-elapsed').textContent = '-';
  $('live-interval').textContent = '-';
  $('live-fps').textContent = '-';
  $('live-draw').textContent = '-';
  $('live-overflow').textContent = '-';
  $('live-first-submit').textContent = '-';
  $('live-first-visible').textContent = '-';
  $('live-initial-window').textContent = '0/30';
  $('scene-phase').textContent = '加载中';
  $('scene-message').textContent = '正在加载测量场景…';
  controls();
  updateSceneCount(state.config.count);
  setView('scene');
  showError('');
  ui.frame.src = pageUrl(state.config, true);
  state.timer = setInterval(() => poll(generation), 250);
}

function cancelRun(message = '') {
  if (!state.running) return;
  clearInterval(state.timer);
  state.timer = null;
  ++state.generation;
  state.running = false;
  updateSceneCount(ui.count.value);
  state.adaptive = null;
  controls();
  reloadPreview();
  showError(message);
}

function reset() {
  if (state.running) return;
  state.engineMode = 'aot';
  state.testIntent = 'steady-state';
  ui.backend.value = 'webgl';
  state.packageSelections = {};
  syncPackageSelector();
  ui.scene.value = 'motionmark.multiply';
  ui.count.value = '512';
  ui.targetFps.value = '60';
  ui.measurement.value = '5000';
  state.antialias = true;
  state.stroke = false;
  ui.antialias.value = 'true';
  ui.stroke.value = 'false';
  state.runMode = 'ramp';
  controls();
  reloadPreview();
}

document.querySelectorAll('[data-view]').forEach(button =>
  button.addEventListener('click', () => setView(button.dataset.view)));
document.querySelectorAll('[data-test-intent]').forEach(button =>
  button.addEventListener('click', () => {
    if (state.running || state.starting ||
        button.dataset.testIntent === state.testIntent) return;
    state.testIntent = button.dataset.testIntent;
    if (state.testIntent === 'cold-start') {
      ui.scene.value = 'coldstart.shader-mix';
      ui.count.value = String(defaults[ui.scene.value]);
    } else if (ui.scene.value === 'coldstart.shader-mix') {
      ui.scene.value = 'motionmark.multiply';
      ui.count.value = String(defaults[ui.scene.value]);
    }
    controls();
    updateSceneHeader();
    void reloadPreview();
  }));
document.querySelectorAll('[data-run-mode]').forEach(button =>
  button.addEventListener('click', () => {
    state.runMode = button.dataset.runMode;
    if (state.runMode === 'ramp') ui.count.removeAttribute('max');
    else ui.count.max = String(limits[ui.scene.value] || 2048);
    controls();
    updateSceneHeader();
  }));
document.querySelectorAll('[data-engine-mode]').forEach(button =>
  button.addEventListener('click', () => {
    if (state.running || state.starting || button.dataset.engineMode === state.engineMode) return;
    state.engineMode = button.dataset.engineMode;
    syncPackageSelector();
    controls();
    updateSceneHeader();
    void reloadPreview();
  }));
ui.scene.addEventListener('change', () => {
  ui.count.value = String(defaults[ui.scene.value] || 24);
  if (coldStartScenes.has(ui.scene.value)) state.testIntent = 'cold-start';
  controls();
  reloadPreview();
});
ui.antialias.addEventListener('change', () => {
  state.antialias = ui.antialias.value === 'true';
  if (!state.running && !state.starting) void reloadPreview();
});
ui.stroke.addEventListener('change', () => {
  state.stroke = ui.stroke.value === 'true';
  if (!state.running && !state.starting) void reloadPreview();
});
document.querySelectorAll('[data-scene]').forEach(button =>
  button.addEventListener('click', () => {
    if (state.running || state.starting) return;
    if (button.dataset.scene === ui.scene.value) {
      setView('scene');
      return;
    }
    ui.scene.value = button.dataset.scene;
    ui.scene.dispatchEvent(new Event('change', {bubbles: true}));
  }));
ui.packageSource.addEventListener('change', () => {
  state.packageSelections[currentBasePackageId()] = ui.packageSource.value;
  void reloadPreview();
});
ui.backend.addEventListener('change', () => {
  syncPackageSelector();
  void reloadPreview();
});
for (const input of [ui.count])
  input.addEventListener('change', reloadPreview);
ui.refreshPackages.addEventListener('click', async () => {
  if (state.running || state.starting) return;
  try {
    await loadPackageCatalog();
    await reloadPreview();
  } catch (error) {
    showError(error.message);
  }
});
for (const input of [ui.targetFps, ui.measurement])
  input.addEventListener('change', controls);
ui.run.addEventListener('click', startRun);
ui.stop.addEventListener('click', () => endRun(true));
ui.cancel.addEventListener('click', () => cancelRun());
ui.reset.addEventListener('click', reset);
$('download').addEventListener('click', () => {
  if (!state.report) return;
  const blob = new Blob([JSON.stringify(state.report, null, 2)], {type: 'application/json'});
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  const identity = state.report.tgfx;
  const commit = identity?.commit ? `-${shortCommit(identity.commit)}` : '';
  anchor.download = `${state.report.tgfxPackage || 'tgfx-benchmark'}${commit}-${Date.now()}.json`;
  anchor.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});
$('load').addEventListener('change', async event => {
  const file = event.target.files?.[0];
  if (!file) return;
  try {
    const report = JSON.parse(await file.text());
    if (!report || typeof report !== 'object' || !report.backend || !report.caseId ||
        !Number.isFinite(report.sampleCount)) throw new Error('无法识别的报告格式。');
    report.testIntent = report.testIntent === 'cold-start' ? 'cold-start' : 'steady-state';
    state.report = report;
    state.history.unshift(report);
    $('report-preview').removeAttribute('src');
    $('report-preview').classList.add('hidden');
    renderReport();
    controls();
    setView('report');
  } catch (error) {
    showError(`无法导入报告：${error.message}`);
  }
  event.target.value = '';
});
ui.frame.addEventListener('load', () => {
  const win = frameWindow();
  const generation = state.generation;
  const handleFrameError = message => {
    if (state.running && generation === state.generation) {
      cancelRun(`渲染发生错误，测量已取消：${message}`);
    } else if (!state.running && win === frameWindow()) {
      $('scene-phase').textContent = '加载失败';
      $('scene-message').textContent = message;
      showError(`预览发生错误：${message}`);
    }
  };
  win?.addEventListener('error', event => handleFrameError(event.message));
  win?.addEventListener('unhandledrejection', event =>
    handleFrameError(event.reason?.message || String(event.reason)));
  if (!state.running) $('scene-message').textContent = '正在加载场景…';
  updateResolution();
});
let stageSize = null;
new ResizeObserver(() => {
  const {width, height} = ui.stage.getBoundingClientRect();
  if (state.running && stageSize &&
      (Math.abs(width - stageSize.width) > 1 || Math.abs(height - stageSize.height) > 1)) {
    cancelRun('测量期间演示区尺寸发生变化，已停止测试。请保持窗口尺寸不变后重新开始。');
  }
  stageSize = {width, height};
}).observe(ui.stage);
window.addEventListener('resize', drawChart);
setInterval(() => {
  if (state.running || state.view !== 'scene') return;
  try {
    const benchmark = frameWindow()?.__tgfxBenchmark;
    if (benchmark?.initializationError) {
      $('scene-phase').textContent = '加载失败';
      $('scene-message').textContent = benchmark.initializationError;
      showError(`预览加载失败：${benchmark.initializationError}`);
      return;
    }
    const stats = sampleStats();
    if (stats && benchmark?.firstDrawAt != null) {
      $('scene-phase').textContent = '预览';
      $('scene-message').textContent = coldStartScenes.has(ui.scene.value)
        ? `动画预览 · ${stats.sceneCount} 个绘制单元 · 测试初始数量 ${ui.count.value}`
        : '正在预览';
      updateSceneCount(stats.sceneCount || ui.count.value);
      updateHud(stats);
      updateResolution();
    }
  } catch (error) {
    // Preview may be reloading while the selected backend changes.
  }
}, 500);
controls();
updateSceneHeader();
async function initializePreview() {
  try {
    await loadPackageCatalog();
  } catch (error) {
    showError(error.message);
  }
  if (state.generation === 0 && !state.running && !state.starting) void reloadPreview();
}
void initializePreview();
