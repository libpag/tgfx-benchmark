#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const {execFileSync} = require('node:child_process');

const root = path.resolve(__dirname, '../..');
const cpp = fs.readFileSync(path.join(root, 'web/demo/src/TGFXBaseView.cpp'), 'utf8');
const header = fs.readFileSync(path.join(root, 'web/demo/src/TGFXBaseView.h'), 'utf8');
let methods = cpp.slice(cpp.indexOf('bool TGFXBaseView::getAdaptiveWindowStats'),
  cpp.indexOf('void TGFXBaseView::stopBenchmarkRun'));
if (process.argv.includes('--negative-control')) {
  methods = methods.replace('(state.adaptiveSettled && !state.adaptiveTargetReached)',
    'state.adaptiveSettled');
}
const types = header.slice(header.indexOf('  struct FrameSample'),
  header.indexOf(' private:'));
const stopMethod = cpp.slice(cpp.indexOf('void TGFXBaseView::stopBenchmarkRun'),
  cpp.indexOf('bool TGFXBaseView::isBenchmarkComplete'));
const harness = `
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <memory>
#include <vector>
#undef assert
#define assert(condition) if (!(condition)) throw std::runtime_error(#condition)
namespace tgfx {
  struct Clock { static int64_t Now() { return 1000000000; } };
}
namespace emscripten {
  struct val {
    static val global(const char*) { return {}; }
    bool hasOwnProperty(const char*) { return false; }
    template<class T> T call(const char*, const std::string&) { return T{}; }
  };
}
struct Context { void submit(std::shared_ptr<int>) {} };
struct Device {
  Context* lockContext() { return nullptr; }
  void unlock() {}
};
struct Window { Device* getDevice() { return nullptr; } };
class TGFXBaseView {
 public:
  ${types}
  size_t sceneCount = 100;
  void applyBenchmarkSceneCount(size_t count) {
    sceneCount = count;
    benchmarkState.adaptiveCountChangedUs = now;
    benchmarkState.adaptiveLastEvaluatedFrameId = benchmarkState.nextFrameId - 1;
    benchmarkState.adaptiveConsecutiveFailures = 0;
  }
  bool getAdaptiveWindowStats(float&, int64_t&);
  void updateAdaptiveBenchmark(int64_t);
  size_t nextAdaptiveCandidateCount() const;
  void startAdaptiveCountTransition(size_t, int64_t);
  void stopBenchmarkRun();
  std::string getBenchmarkSummaryJson() const { return "{}"; }
  Window* window = nullptr;
  std::shared_ptr<int> lastRecording;
  int64_t now = 100000;
  void initialize() {
    benchmarkState.active = true;
    benchmarkState.adaptive = true;
    benchmarkState.adaptiveStabilizing = false;
    benchmarkState.adaptiveTargetFps = 60.0f;
    benchmarkState.measurementStarted = true;
    benchmarkState.measurementStartUs = 1000;
    benchmarkState.adaptiveStableWindowUs = 1500000;
  }
  void feed(int frames, int64_t interval = 16667) {
    for (int i = 0; i < frames; ++i) {
      now += interval;
      auto id = benchmarkState.nextFrameId++;
      FrameSample sample{id, id, id - 1, now, now + 100, 50, 30, 20, sceneCount};
      auto& state = benchmarkState;
      if (state.samples.size() < 8192) state.samples.push_back(sample);
      else {
        state.overflowed = true;
        state.samples[state.recordedFrameCount % 8192] = sample;
      }
      ++state.recordedFrameCount;
      updateAdaptiveBenchmark(now);
    }
  }
  void feedResponsive(int frames) {
    for (int i = 0; i < frames; ++i) {
      const auto fps = std::min(120.0f, 8000.0f / static_cast<float>(sceneCount));
      feed(1, static_cast<int64_t>(1000000.0f / fps));
    }
  }
};
${methods}
${stopMethod}
int runTests() {
  TGFXBaseView v;
  v.initialize();
  v.feedResponsive(4000);
  assert(v.benchmarkState.adaptiveTargetReached);
  assert(v.sceneCount > 115);
  assert(v.sceneCount < 145);
  assert(v.benchmarkState.adaptiveWindowFps <= 63.0f);
  assert(v.benchmarkState.adaptiveLowPassingCount > 0);
  assert(v.benchmarkState.adaptiveHighFailingCount >=
         v.benchmarkState.adaptiveLowPassingCount);
  assert(v.benchmarkState.adaptiveSearchIteration > 3);
  auto losses = v.benchmarkState.adaptiveStabilityLossCount;
  v.feed(60, 20000);
  assert(!v.benchmarkState.adaptiveTargetReached);
  assert(v.benchmarkState.adaptiveStableStartUs == 0);
  assert(v.benchmarkState.adaptiveStabilityLossCount == losses + 1);
  v.feedResponsive(3000);
  assert(v.benchmarkState.adaptiveTargetReached);
  auto windowCount = v.benchmarkState.adaptiveWindowCount;
  v.feed(9000);
  assert(v.benchmarkState.overflowed);
  assert(v.benchmarkState.adaptiveWindowCount > windowCount + 100);
  TGFXBaseView mixed;
  mixed.initialize();
  mixed.feed(60);
  auto mixedWindowCount = mixed.benchmarkState.adaptiveWindowCount;
  mixed.applyBenchmarkSceneCount(120);
  mixed.feed(65);
  assert(mixed.benchmarkState.adaptiveWindowCount == mixedWindowCount + 1);
  mixed.feed(31);
  assert(mixed.benchmarkState.adaptiveWindowCount == mixedWindowCount + 2);
  TGFXBaseView transient;
  transient.initialize();
  transient.feedResponsive(1000);
  transient.feed(1, 300000);
  transient.feed(59);
  assert(transient.benchmarkState.adaptiveStableStartUs == 0 ||
         !transient.benchmarkState.adaptiveWindowPassing);
  transient.feed(60);
  assert(transient.sceneCount > 115);
  TGFXBaseView growth;
  growth.initialize();
  growth.feedResponsive(31);
  assert(growth.sceneCount == 100);
  assert(growth.benchmarkState.adaptiveGrowthTo > growth.sceneCount);
  growth.feed(12, 8333);
  assert(growth.sceneCount > 100);
  assert(growth.sceneCount < growth.benchmarkState.adaptiveGrowthTo);
  assert(growth.benchmarkState.adaptiveWindowCount == 1);
  growth.feed(13, 8333);
  assert(growth.sceneCount == growth.benchmarkState.adaptiveGrowthTo);
  TGFXBaseView headroom;
  headroom.initialize();
  headroom.feedResponsive(4000);
  headroom.sceneCount = 1312;
  headroom.benchmarkState.adaptiveSettled = false;
  headroom.benchmarkState.adaptiveTargetReached = false;
  headroom.benchmarkState.adaptiveStabilizing = true;
  headroom.benchmarkState.adaptiveLowPassingCount = 1312;
  headroom.benchmarkState.adaptiveHighFailingCount = 1377;
  headroom.benchmarkState.adaptiveLowPassingFps = 120.0f;
  headroom.benchmarkState.adaptiveHighFailingFps = 92.0f;
  headroom.benchmarkState.adaptiveStableStartUs = headroom.now - 2000000;
  headroom.feed(61, 8333);
  assert(headroom.benchmarkState.adaptiveGrowthTo > 1312);
  assert(headroom.benchmarkState.adaptiveHighFailingCount == 0);
  assert(!headroom.benchmarkState.adaptiveSettled);
  TGFXBaseView preservedBracket;
  preservedBracket.initialize();
  preservedBracket.sceneCount = 16;
  preservedBracket.benchmarkState.adaptiveStabilizing = true;
  preservedBracket.benchmarkState.adaptiveLowPassingCount = 16;
  preservedBracket.benchmarkState.adaptiveHighFailingCount = 20;
  preservedBracket.benchmarkState.adaptiveLowPassingFps = 91.0f;
  preservedBracket.benchmarkState.adaptiveHighFailingFps = 49.0f;
  preservedBracket.feed(61, 11000);
  assert(preservedBracket.benchmarkState.adaptiveHighFailingCount == 20);
  assert(preservedBracket.benchmarkState.adaptiveGrowthTo > 16);
  assert(preservedBracket.benchmarkState.adaptiveGrowthTo < 20);
  TGFXBaseView contradictoryBoundary;
  contradictoryBoundary.initialize();
  contradictoryBoundary.benchmarkState.adaptiveStabilizing = true;
  contradictoryBoundary.benchmarkState.adaptiveLowPassingCount = 100;
  contradictoryBoundary.benchmarkState.adaptiveHighFailingCount = 100;
  contradictoryBoundary.feed(61);
  assert(contradictoryBoundary.benchmarkState.adaptiveGrowthTo > 100);
  assert(contradictoryBoundary.benchmarkState.adaptiveHighFailingCount == 0);
  contradictoryBoundary.benchmarkState.adaptiveGrowthStartUs = 0;
  contradictoryBoundary.benchmarkState.adaptiveStabilizing = true;
  contradictoryBoundary.benchmarkState.adaptiveLowPassingCount = 100;
  contradictoryBoundary.benchmarkState.adaptiveHighFailingCount = 101;
  contradictoryBoundary.feed(60, 20000);
  assert(contradictoryBoundary.benchmarkState.adaptiveGrowthTo < 100);
  contradictoryBoundary.feed(20);
  assert(contradictoryBoundary.sceneCount < 100);
  assert(contradictoryBoundary.benchmarkState.adaptiveLowPassingCount == 0);
  TGFXBaseView finalTail;
  finalTail.initialize();
  finalTail.feedResponsive(4000);
  assert(finalTail.benchmarkState.adaptiveTargetReached);
  finalTail.benchmarkState.adaptiveLastEvaluatedFrameId =
      finalTail.benchmarkState.nextFrameId + 100000;
  finalTail.feed(120, 100000);
  assert(finalTail.benchmarkState.adaptiveTargetReached);
  finalTail.stopBenchmarkRun();
  assert(!finalTail.benchmarkState.adaptiveTargetReached);
  assert(finalTail.benchmarkState.complete);
  TGFXBaseView overload;
  overload.initialize();
  overload.feed(17, 150000);
  assert(overload.benchmarkState.adaptiveHighFailingCount == 100);
  assert(overload.benchmarkState.adaptiveGrowthTo < 100);
  TGFXBaseView unreachable;
  unreachable.initialize();
  unreachable.sceneCount = 1;
  unreachable.feed(220, 40000);
  assert(unreachable.benchmarkState.adaptiveSettled);
  assert(!unreachable.benchmarkState.adaptiveTargetReached);
  assert(unreachable.benchmarkState.adaptiveSettledCount == 1);
  TGFXBaseView quick;
  quick.initialize();
  quick.benchmarkState.adaptiveQuick = true;
  quick.sceneCount = 100;
  quick.feed(61, 30000);
  assert(quick.benchmarkState.adaptiveSettled);
  assert(quick.benchmarkState.adaptiveThresholdReached);
  assert(!quick.benchmarkState.adaptiveTargetReached);
  assert(quick.benchmarkState.adaptiveSettledCount == 100);
  TGFXBaseView capped;
  capped.initialize();
  capped.feedResponsive(65);
  for (int i = 0; i < 3; ++i) {
    capped.sceneCount = 100;
    capped.benchmarkState.adaptiveGrowthFrom = 100;
    capped.benchmarkState.adaptiveGrowthTo = 100;
    capped.benchmarkState.adaptiveGrowthStartUs = capped.now - 200000;
    capped.feed(1);
  }
  assert(capped.benchmarkState.adaptiveCapacityLimited);
  std::cout << "adaptive: bracketed search, boundary recovery, revalidation, ring buffer passed\\n";
  return 0;
}
int main() {
  try { return runTests(); }
  catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << "\\n";
    return 1;
  }
}
`;

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'tgfx-adaptive-'));
try {
  const source = path.join(temporary, 'test.cpp');
  const executable = path.join(temporary, 'test');
  fs.writeFileSync(source, harness);
  execFileSync(process.env.CXX || 'clang++', ['-std=c++17', source, '-o', executable],
    {stdio: 'inherit'});
  if (process.argv.includes('--negative-control')) {
    assert.throws(() => execFileSync(executable, {stdio: 'pipe'}),
      error => error.status === 1 &&
        error.stderr.toString().includes('!v.benchmarkState.adaptiveTargetReached'));
    console.log('negative control: old stop-checking-after-settlement behavior correctly fails');
  } else {
    execFileSync(executable, {stdio: 'inherit'});
  }
} finally {
  fs.rmSync(temporary, {recursive: true, force: true});
}

const ui = fs.readFileSync(path.join(root, 'web/demo/benchmark-ui.js'), 'utf8');
const context = vm.createContext({
  navigator: {platform: 'test', userAgent: 'test'},
  window: {devicePixelRatio: 1},
  frameWindow: () => null,
});
const shaderMixGroupsStart = ui.indexOf('const shaderMixGroups');
const shaderMixGroupsEnd = ui.indexOf('];', shaderMixGroupsStart) + 2;
assert.ok(shaderMixGroupsStart >= 0 && shaderMixGroupsEnd > shaderMixGroupsStart);
vm.runInContext(ui.slice(shaderMixGroupsStart, shaderMixGroupsEnd), context);
vm.runInContext(ui.slice(ui.indexOf('function formatMs'), ui.indexOf('function drawRampChart')),
  context);
vm.runInContext(ui.slice(ui.indexOf('function runtimeEvidenceLabel'),
  ui.indexOf('function renderSessionHistory')), context);
assert.equal(context.runtimeEvidenceLabel({
  tgfx: {codegen: 'jit'},
}), 'JIT');
assert.match(context.runtimeEvidenceLabel({
  tgfx: {
    codegen: 'aot',
    runtimeValidity: true,
    runtime: {isLoaded: true, hitCount: 6, missCount: 0, failureCount: 0, hitSummary: [{}, {}]},
  },
}), /hit 6 \/ miss 0 · 2 类/);
assert.match(context.runtimeEvidenceLabel({
  tgfx: {
    codegen: 'aot',
    runtimeValidity: false,
    runtime: {isLoaded: true, hitCount: 5, missCount: 1, failureCount: 1},
  },
}), /^不完整 · hit 5 \/ miss 1 \/ fail 1$/);
const summary = {
  adaptive: true, adaptiveTargetReached: false, adaptiveStableStartUs: 1000,
  adaptiveTargetFps: 60, adaptiveSettledCount: 100,
  finalSceneCount: 100,
  stopReason: 'adaptive-unsettled-user-stop',
  samples: [0, 1, 2].map(i => ({callbackBeginUs: 1000 + 16667 * i, sceneCount: 100})),
};
const identity = {
  packageId: 'jit-webgl-test',
  backend: 'webgl',
  codegen: 'jit',
  commit: '0123456789abcdef0123456789abcdef01234567',
  dirty: false,
  sourceName: 'test',
  builtAt: null,
};
const options = {
  mode: 'ramp', targetFps: 60, count: 100,
  backend: 'webgl', caseId: 'motionmark.multiply',
  sceneName: 'Multiply', engineMode: 'jit',
  tgfxPackage: identity.packageId, tgfxIdentity: identity,
};
let report = context.summarize(summary, options);
assert.equal(report.sampleCount, 3, 'manual stop must retain the latest same-count window');
assert.ok(Number.isFinite(report.intervalP95Ms));
assert.equal(report.analysisKind, 'current-load');
assert.equal(report.count, 100, 'unsettled report must retain the final actual count');
assert.equal(report.adaptive.finalCount, 100);
let warnings = context.reportWarnings(report);
assert.equal(warnings.length, 0, 'manual ramp stop must be informational');
assert.match(context.rampVerdict(report), /手动结束/);
report.stopReason = 'adaptive-unknown-failure';
warnings = context.reportWarnings(report);
assert.equal(warnings.length, 1);
assert.match(warnings[0], /异常结束/);
summary.adaptiveTargetReached = true;
summary.samples.push({callbackBeginUs: 60000, sceneCount: 120});
report = context.summarize(summary, options);
assert.equal(report.sampleCount, 3, 'mixed-load sample must not enter steady report');
assert.ok(Math.abs(report.measuredFps - 60) < 0.01);
assert.equal(report.rawSamples.length, 4);
warnings = context.reportWarnings(report);
assert.equal(warnings.length, 1);
assert.match(warnings[0], /置信度较低/);
report.sampleCount = 40;
assert.equal(context.reportWarnings(report).length, 0);
summary.adaptiveThresholdReached = true;
summary.adaptiveTargetReached = false;
summary.adaptiveSettledCount = 137;
summary.finalSceneCount = 137;
summary.stopReason = 'adaptive-threshold-user-stop';
summary.samples = [0, 1, 2, 3].map(i => ({
  callbackBeginUs: 1000 + 16667 * i,
  sceneCount: 137,
}));
report = context.summarize(summary, options);
assert.equal(report.analysisKind, 'threshold-estimate');
assert.equal(report.sampleCount, 4);
assert.equal(context.reportWarnings(report).length, 0);
assert.match(context.rampVerdict(report), /估算容量/);
summary.overflowed = true;
summary.sampleCount = 8192;
summary.recordedFrameCount = 12000;
report = context.summarize(summary, options);
assert.equal(report.samplePolicy, 'latest-8192');
assert.equal(context.reportWarnings(report).length, 0);

const coldSummary = {
  testIntent: 'cold-start',
  sceneVersion: 'v7',
  executionStatus: 'completed',
  finalSceneCount: 256,
  firstSubmitUs: 120000,
  coldStartFirstVisibleUs: 150000,
  coldStartFramesAfterVisible: 30,
  warmupDurationUs: 0,
  warmupFrameCount: 0,
  measurementDurationUs: 700000,
  sampleCount: 31,
  recordedFrameCount: 31,
  overflowed: false,
  firstFrame: {
    available: true,
    reason: null,
    callbackUs: 41000,
    sceneDrawUs: 31000,
    flushUs: 2100,
    submitUs: 321,
  },
  samples: Array.from({length: 31}, (_, index) => ({
    callbackBeginUs: 100000 + index * 16667,
    callbackEndUs: 100000 + index * 16667 + (index === 0 ? 40000 : 12000),
    sceneDrawUs: index === 0 ? 30000 : 8000,
    flushUs: 2000,
    submitUs: index === 0 ? 1000 : 100,
    sceneCount: 256,
  })),
};
report = context.summarize(coldSummary, {...options, caseId: 'coldstart.shader-mix'});
assert.equal(report.testIntent, 'cold-start');
assert.equal(report.mode, 'cold-start');
assert.equal(report.count, 256);
assert.equal(report.sceneVersion, 'v7');
assert.equal(report.coldStart.initialWindowFrames, 30);
assert.equal(report.coldStart.firstSubmitMs, 120);
assert.equal(report.coldStart.firstVisibleMs, 150);
assert.equal(report.coldStart.firstFrame.available, true);
assert.equal(report.coldStart.firstFrame.reason, null);
assert.equal(report.coldStart.firstFrame.callbackUs, 41000);
assert.equal(report.coldStart.firstFrame.sceneDrawUs, 31000);
assert.equal(report.coldStart.firstFrame.flushUs, 2100);
assert.equal(report.coldStart.firstFrame.submitUs, 321);
assert.equal(report.coldStart.firstFrameCallbackMs, 41);
assert.equal(report.coldStart.firstFrameSceneDrawMs, 31);
assert.equal(report.coldStart.firstFrameFlushMs, 2.1);
assert.equal(report.coldStart.firstFrameSubmitUs, 0.321);
assert.equal(report.shaderGroups.length, 14);
assert.ok(report.shaderGroups.includes('blur-shadow-mask-filter'));
assert.ok(report.shaderGroups.includes('batch-instancing'));
assert.equal(report.adaptive, null);
assert.equal(context.reportWarnings(report).length, 0);
assert.equal(context.summarize({...coldSummary, sceneVersion: undefined},
  {...options, caseId: 'coldstart.shader-mix'}).sceneVersion, null,
  'old package results must not inherit the new scene revision');
const fallbackReport = context.summarize(
  {...coldSummary, firstFrame: undefined},
  {...options, caseId: 'coldstart.shader-mix'},
);
assert.equal(fallbackReport.coldStart.firstFrame.available, true);
assert.equal(fallbackReport.coldStart.firstFrame.callbackUs, 40000);
assert.equal(fallbackReport.coldStart.firstFrame.sceneDrawUs, 30000);
assert.equal(fallbackReport.coldStart.firstFrame.flushUs, 2000);
assert.equal(fallbackReport.coldStart.firstFrame.submitUs, 1000);

const legacyReport = context.summarize({...coldSummary, testIntent: undefined}, options);
assert.equal(legacyReport.testIntent, 'steady-state');
console.log('report: cold/steady separation, ramp filtering and FPS passed');
