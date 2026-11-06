/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "TGFXBaseView.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <emscripten/html5.h>
#include <emscripten/val.h>
#include <iostream>
#include "base/Bench.h"
#include "tgfx/core/Clock.h"

#if __has_include("gpu/PrecompiledShaderCache.h")
#include "gpu/PrecompiledShaderCache.h"
#define TGFX_BENCHMARK_HAS_AOT_CACHE 1
#else
#define TGFX_BENCHMARK_HAS_AOT_CACHE 0
#endif

using namespace emscripten;

static constexpr int64_t FLUSH_INTERVAL = 300000;
static constexpr size_t MAX_FRAME_SAMPLES = 8192;
static constexpr uint64_t STEADY_WARMUP_FRAMES = 10;
static constexpr int32_t DEFAULT_COLD_START_WINDOW_FRAMES = 30;

namespace benchmark {

template <typename T>
static std::string HexValue(T value, int width) {
  std::ostringstream stream;
  stream << "0x" << std::hex << std::nouppercase << std::setw(width) << std::setfill('0')
         << static_cast<uint64_t>(value);
  return stream.str();
}

EM_BOOL RequestFrameCallback(double, void* userData) {
  auto baseView = static_cast<TGFXBaseView*>(userData);
  if (baseView) {
    baseView->draw();
  }
  return baseView && baseView->isBenchmarkComplete() ? EM_FALSE : EM_TRUE;
}

EM_BOOL MouseClickCallback(int, const EmscriptenMouseEvent* e, void* userData) {
  auto baseView = static_cast<TGFXBaseView*>(userData);
  if (baseView) {
    const float x = static_cast<float>(e->targetX) * baseView->mouseScaleX;
    const float y = static_cast<float>(e->targetY) * baseView->mouseScaleY;
    baseView->appHost->mouseMoved(x, y);
    baseView->appHost->resetFrames();
    baseView->drawIndex++;
  }
  return EM_TRUE;
}

EM_BOOL MouseMoveCallBack(int, const EmscriptenMouseEvent* e, void* userData) {
  auto baseView = static_cast<TGFXBaseView*>(userData);
  if (baseView) {
    const float x = static_cast<float>(e->targetX) * baseView->mouseScaleX;
    const float y = static_cast<float>(e->targetY) * baseView->mouseScaleY;
    baseView->appHost->mouseMoved(x, y);
  }
  return EM_TRUE;
}

EM_BOOL MouseLeaveCallBack(int, const EmscriptenMouseEvent*, void* userData) {
  auto appHost = static_cast<benchmark::AppHost*>(userData);
  if (appHost) {
    appHost->mouseMoved(-1, -1);
  }
  return EM_TRUE;
}

TGFXBaseView::TGFXBaseView(const std::string& canvasID) : canvasID(canvasID) {
  // Clock::Now() establishes its process-local origin on first use; do it before benchmark timing.
  tgfx::Clock::Now();
  appHost = std::make_shared<benchmark::AppHost>(1024, 720);
  drawIndex = 0;
  emscripten_set_click_callback(canvasID.c_str(), this, EM_TRUE, MouseClickCallback);
  emscripten_set_mousemove_callback(canvasID.c_str(), this, EM_TRUE, MouseMoveCallBack);
  emscripten_set_mouseleave_callback(canvasID.c_str(), appHost.get(), EM_TRUE, MouseLeaveCallBack);
}

void TGFXBaseView::updateSize(float devicePixelRatio) {
  if (!canvasID.empty()) {
    int width = 0;
    int height = 0;
    emscripten_get_canvas_element_size(canvasID.c_str(), &width, &height);
    double cssWidth = 0;
    double cssHeight = 0;
    if (emscripten_get_element_css_size(canvasID.c_str(), &cssWidth, &cssHeight) == EMSCRIPTEN_RESULT_SUCCESS) {
      mouseScaleX = cssWidth > 0 ? static_cast<float>(width / cssWidth) : 1.0f;
      mouseScaleY = cssHeight > 0 ? static_cast<float>(height / cssHeight) : 1.0f;
    }
    if (appHost->updateScreen(width, height, devicePixelRatio)) {
      surface = nullptr;
    }
  }
}

void TGFXBaseView::startDraw() {
  ParticleBench::ShowPerfData(showPerfDataFlag);
  emscripten_request_animation_frame_loop(RequestFrameCallback, this);
}

void TGFXBaseView::setImagePath(const std::string& imagePath) {
  auto image = tgfx::Image::MakeFromFile(imagePath.c_str());
  if (image) {
    appHost->addImage("bridge", std::move(image));
  }
}

bool TGFXBaseView::setImageSource(tgfx::NativeImageRef nativeImage) {
  return setNamedImageSource("bridge", std::move(nativeImage));
}

bool TGFXBaseView::setNamedImageSource(const std::string& name,
                                       tgfx::NativeImageRef nativeImage) {
  if (name.empty()) {
    return false;
  }
  auto image = tgfx::Image::MakeFrom(std::move(nativeImage));
  if (image == nullptr) {
    return false;
  }
  appHost->addImage(name, std::move(image));
  return true;
}

void TGFXBaseView::draw() {
  const auto callbackBeginUs = tgfx::Clock::Now();
  const auto frameId = benchmarkState.nextFrameId++;
  const auto submittedFrameId = benchmarkState.lastRecordingFrameId;
  int64_t sceneDrawUs = 0;
  int64_t flushUs = 0;
  int64_t submitUs = 0;
  if (appHost->width() <= 0 || appHost->height() <= 0) {
    return;
  }
  if (window == nullptr) {
    window = MakeTGFXWindow(canvasID);
  }
  if (window == nullptr) {
    return;
  }
  auto device = window->getDevice();
  if (device == nullptr) {
    return;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
#if TGFX_BENCHMARK_HAS_AOT_CACHE
  if (auto* cache = context->precompiledShaderCache()) {
    cache->setDiagnosticRecordingEnabled(recorderEnabled);
  }
#endif
  if (surface == nullptr) {
    if (lastRecording != nullptr) {
      context->submit(std::move(lastRecording));
    }
    surface = tgfx::Surface::MakeFrom(context, window);
  }
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->clear({0.87f, 0.87f, 0.87f, 1.0f});
  auto bench = benchmarkBench != nullptr ? benchmarkBench : benchmark::Bench::GetByIndex(drawIndex % benchmark::Bench::Count());
  const auto sceneDrawStartUs = tgfx::Clock::Now();
  bench->draw(canvas, appHost.get());
  sceneDrawUs = tgfx::Clock::Now() - sceneDrawStartUs;
  if (!showPerfDataFlag) {
    if (bench == benchmark::Bench::GetByStableId("legacy.particles-rect")) {
      const auto particleBench = static_cast<benchmark::ParticleBench*>(bench);
      updatePerfInfo(particleBench->getPerfData());
    }
  }
  const auto flushStartUs = tgfx::Clock::Now();
  auto recording = context->flush();
  flushUs = tgfx::Clock::Now() - flushStartUs;
  std::swap(lastRecording, recording);
  benchmarkState.lastRecordingFrameId = frameId;
  if (recording != nullptr) {
    const auto submitStartUs = tgfx::Clock::Now();
    context->submit(std::move(recording));
    submitUs = tgfx::Clock::Now() - submitStartUs;
    if (benchmarkState.active && benchmarkState.firstSubmitUs == 0) {
      benchmarkState.firstSubmitUs = tgfx::Clock::Now() - benchmarkState.startUs;
    }
  }
  device->unlock();
  const auto callbackEndUs = tgfx::Clock::Now();
  lastFrameIntervalUs = lastFrameBeginUs == 0 ? 0 : callbackBeginUs - lastFrameBeginUs;
  lastFrameBeginUs = callbackBeginUs;
  lastFrameCallbackUs = callbackEndUs - callbackBeginUs;
  appHost->recordFrame(callbackEndUs - callbackBeginUs);
  if (benchmarkState.active) {
    ++benchmarkState.successfulFrameCount;
  }
  if (benchmarkState.active && !benchmarkState.coldStart &&
      !benchmarkState.measurementStarted &&
      benchmarkState.successfulFrameCount > STEADY_WARMUP_FRAMES) {
    benchmarkState.measurementStarted = true;
    benchmarkState.steadyWarmupFrameCount = STEADY_WARMUP_FRAMES;
    benchmarkState.measurementStartUs = callbackBeginUs;
    benchmarkState.measurementEndUs =
        benchmarkState.requestedMeasurementUs > 0
            ? benchmarkState.measurementStartUs + benchmarkState.requestedMeasurementUs
            : std::numeric_limits<int64_t>::max();
  }
  if (benchmarkState.active && recorderEnabled &&
      (benchmarkState.coldStart || benchmarkState.measurementStarted)) {
    const auto sample = FrameSample{
        frameId, frameId, submittedFrameId, callbackBeginUs, callbackEndUs,
        sceneDrawUs, flushUs, submitUs, sceneCount};
    if (benchmarkState.samples.size() < MAX_FRAME_SAMPLES) {
      benchmarkState.samples.push_back(sample);
    } else {
      benchmarkState.overflowed = true;
      benchmarkState.samples[benchmarkState.recordedFrameCount % MAX_FRAME_SAMPLES] = sample;
    }
    ++benchmarkState.recordedFrameCount;
  }
  if (benchmarkState.active && benchmarkState.coldStart &&
      benchmarkState.coldStartVisibleFrameId > 0 &&
      frameId > benchmarkState.coldStartVisibleFrameId) {
    ++benchmarkState.coldStartFramesAfterVisible;
  }
  if (!benchmarkState.coldStart) {
    updateAdaptiveBenchmark(callbackEndUs);
  }
  if (!firstDrawNotified) {
    auto jsWindow = emscripten::val::global("window");
    if (jsWindow.hasOwnProperty("__tgfxBenchmarkFirstDraw")) {
      jsWindow.call<void>("__tgfxBenchmarkFirstDraw",
                          static_cast<double>(callbackEndUs - callbackBeginUs) / 1000.0);
      firstDrawNotified = true;
    }
  }
  if (benchmarkState.active && benchmarkState.coldStart &&
      benchmarkState.coldStartVisibleFrameId > 0 &&
      benchmarkState.coldStartFramesAfterVisible >=
          static_cast<uint64_t>(benchmarkState.coldStartInitialWindowFrames)) {
    stopBenchmarkRun();
  } else if (benchmarkState.active && benchmarkState.measurementStarted &&
      !benchmarkState.manual && !benchmarkState.coldStart &&
      callbackEndUs >= benchmarkState.measurementEndUs) {
    stopBenchmarkRun();
  }
}

void TGFXBaseView::restartDraw() const {
  if (appHost) {
    appHost->resetFrames();
  }
}

void TGFXBaseView::updatePerfInfo(const PerfData& data) {
  auto jsWindow = emscripten::val::global("window");
  if (!jsWindow.hasOwnProperty("updatePerfInfo")) {
    return;
  }
  static int64_t lastFlushTime = -1;
  const auto currentTime = tgfx::Clock::Now();
  if (lastFlushTime == -1) {
    lastFlushTime = currentTime;
  }
  if (data.fps > 0.0f) {
    if (const auto flushInterval = currentTime - lastFlushTime; flushInterval > FLUSH_INTERVAL) {
      jsWindow.call<void>("updatePerfInfo", data.fps, data.drawTime, data.drawCount, false);
      lastFlushTime = currentTime - (flushInterval % FLUSH_INTERVAL);
    }
  }
}

void TGFXBaseView::updateDrawParam(const DrawParam& drawParam) const {
  ParticleBench::SetInitDrawCount(drawParam.startCount);
  ParticleBench::SetStepDrawCount(drawParam.stepCount);
  ParticleBench::SetMaxDrawCount(drawParam.maxCount);
  ParticleBench::SetTargetFPS(drawParam.minFPS);
  appHost->resetFrames();
}

void TGFXBaseView::updateGraphicType(int type) {
  drawIndex = type;
  appHost->resetFrames();
}

bool TGFXBaseView::setSceneById(const std::string& stableId) {
  auto bench = benchmark::Bench::GetByStableId(stableId);
  if (bench == nullptr) {
    return false;
  }
  benchmarkBench = bench;
  appHost->resetFrames();
  return true;
}

void TGFXBaseView::setSceneCount(size_t count) {
  applyBenchmarkSceneCount(count);
}

void TGFXBaseView::applyBenchmarkSceneCount(size_t count) {
  if (benchmarkBench != nullptr) {
    benchmarkBench->setCount(count);
    const auto actualCount = benchmarkBench->getCount();
    if (sceneCount != actualCount && benchmarkState.active) {
      benchmarkState.adaptiveCountChangedUs = tgfx::Clock::Now();
      benchmarkState.adaptiveLastEvaluatedFrameId = benchmarkState.nextFrameId - 1;
      benchmarkState.adaptiveConsecutiveFailures = 0;
    }
    sceneCount = actualCount;
  } else {
    sceneCount = count;
  }
}

void TGFXBaseView::setRecorderEnabled(bool enabled) {
  recorderEnabled = enabled;
#if TGFX_BENCHMARK_HAS_AOT_CACHE
  auto device = window ? window->getDevice() : nullptr;
  auto context = device ? device->lockContext() : nullptr;
  if (context != nullptr) {
    if (auto* cache = context->precompiledShaderCache()) {
      cache->setDiagnosticRecordingEnabled(enabled);
    }
    device->unlock();
  }
#endif
}

bool TGFXBaseView::startBenchmarkRun(int64_t measurementUs) {
  if (measurementUs <= 0) {
    return false;
  }
  benchmarkState = BenchmarkState{};
  benchmarkState.active = true;
  benchmarkState.startUs = std::max<int64_t>(1, tgfx::Clock::Now());
  benchmarkState.requestedMeasurementUs = measurementUs;
  benchmarkState.manual = false;
  benchmarkState.samples.reserve(MAX_FRAME_SAMPLES);
  return true;
}

bool TGFXBaseView::startManualBenchmarkRun() {
  benchmarkState = BenchmarkState{};
  benchmarkState.active = true;
  benchmarkState.startUs = std::max<int64_t>(1, tgfx::Clock::Now());
  benchmarkState.measurementEndUs = std::numeric_limits<int64_t>::max();
  benchmarkState.manual = true;
  benchmarkState.samples.reserve(MAX_FRAME_SAMPLES);
  return true;
}

bool TGFXBaseView::startAdaptiveBenchmarkRun(int64_t stableWindowUs, float targetFps) {
  if (stableWindowUs <= 0 || !std::isfinite(targetFps) || targetFps <= 0.0f ||
      !recorderEnabled) {
    return false;
  }
  benchmarkState = BenchmarkState{};
  benchmarkState.active = true;
  benchmarkState.startUs = std::max<int64_t>(1, tgfx::Clock::Now());
  benchmarkState.measurementEndUs = std::numeric_limits<int64_t>::max();
  benchmarkState.manual = true;
  benchmarkState.adaptive = true;
  benchmarkState.adaptiveQuick = true;
  benchmarkState.adaptiveTargetFps = targetFps;
  benchmarkState.adaptiveStableWindowUs = stableWindowUs;
  benchmarkState.adaptiveInitialCount = sceneCount;
  benchmarkState.adaptivePeakCount = sceneCount;
  benchmarkState.adaptiveLastPassingCount = 0;
  benchmarkState.adaptiveLowPassingCount = 0;
  benchmarkState.adaptiveCountChangedUs = 0;
  benchmarkState.samples.reserve(MAX_FRAME_SAMPLES);
  return true;
}

bool TGFXBaseView::startColdStartBenchmarkRun(int32_t initialWindowFrames) {
  if (initialWindowFrames <= 0 || !recorderEnabled) {
    return false;
  }
  benchmarkState = BenchmarkState{};
  benchmarkState.active = true;
  benchmarkState.coldStart = true;
  benchmarkState.measurementStarted = true;
  benchmarkState.startUs = std::max<int64_t>(1, tgfx::Clock::Now());
  benchmarkState.measurementStartUs = benchmarkState.startUs;
  benchmarkState.measurementEndUs = std::numeric_limits<int64_t>::max();
  benchmarkState.coldStartInitialWindowFrames = initialWindowFrames;
  benchmarkState.samples.reserve(MAX_FRAME_SAMPLES);
  return true;
}

void TGFXBaseView::markColdStartVisible(int64_t elapsedUs) {
  if (!benchmarkState.active || !benchmarkState.coldStart ||
      benchmarkState.coldStartVisibleFrameId > 0) {
    return;
  }
  benchmarkState.coldStartVisibleFrameId =
      benchmarkState.nextFrameId > 0 ? benchmarkState.nextFrameId - 1 : 0;
  benchmarkState.coldStartFirstVisibleUs = std::max<int64_t>(0, elapsedUs);
}

bool TGFXBaseView::getAdaptiveWindowStats(float& fps, int64_t& p95IntervalUs) {
  constexpr size_t SEARCH_WINDOW_FRAMES = 30;
  constexpr size_t CONFIRM_WINDOW_FRAMES = 60;
  const auto windowFrames =
      benchmarkState.adaptiveStabilizing ? CONFIRM_WINDOW_FRAMES : SEARCH_WINDOW_FRAMES;
  const auto size = benchmarkState.samples.size();
  if (size < 2) {
    return false;
  }
  const auto first =
      benchmarkState.overflowed ? benchmarkState.recordedFrameCount % size : static_cast<size_t>(0);
  const auto sampleAt = [&](size_t index) -> const FrameSample& {
    return benchmarkState.samples[(first + index) % size];
  };
  std::vector<int64_t> intervals;
  intervals.reserve(windowFrames);
  int64_t totalUs = 0;
  for (size_t index = size - 1; index > 0 && intervals.size() < windowFrames; --index) {
    const auto& current = sampleAt(index);
    const auto& previous = sampleAt(index - 1);
    if (current.sceneCount != sceneCount || previous.sceneCount != sceneCount ||
        previous.callbackBeginUs < benchmarkState.adaptiveCountChangedUs + 100000 ||
        current.frameId <= benchmarkState.adaptiveLastEvaluatedFrameId) {
      break;
    }
    const auto intervalUs = current.callbackBeginUs - previous.callbackBeginUs;
    if (intervalUs <= 0) {
      break;
    }
    intervals.push_back(intervalUs);
    totalUs += intervalUs;
  }
  // Recover severe overload without waiting tens of seconds for 60 callbacks.
  // These short windows may only fail; a pass still requires the full window.
  if (intervals.size() < windowFrames &&
      (intervals.size() < 8 || totalUs < 1000000 ||
       totalUs / static_cast<int64_t>(intervals.size()) <=
           static_cast<int64_t>(2000000.0 / benchmarkState.adaptiveTargetFps))) {
    return false;
  }
  std::sort(intervals.begin(), intervals.end());
  const auto p95Index =
      std::min(intervals.size() - 1, static_cast<size_t>(std::ceil(intervals.size() * 0.95f)) - 1);
  p95IntervalUs = intervals[p95Index];
  fps = static_cast<float>(intervals.size() * 1000000.0 / static_cast<double>(totalUs));
  benchmarkState.adaptiveLastEvaluatedFrameId = sampleAt(size - 1).frameId;
  ++benchmarkState.adaptiveWindowCount;
  return true;
}

size_t TGFXBaseView::nextAdaptiveCandidateCount() const {
  const auto& state = benchmarkState;
  const auto budgetUs = 1000000.0 / state.adaptiveTargetFps;
  const auto low = state.adaptiveLowPassingCount;
  const auto high = state.adaptiveHighFailingCount;
  if (low > 0 && high > low) {
    if (high - low <= 1) {
      return low;
    }
    const auto lowCost = std::max(1000000.0 / std::max(1.0f, state.adaptiveLowPassingFps),
                                  static_cast<double>(state.adaptiveLowPassingP95Us));
    const auto highCost = std::max(1000000.0 / std::max(1.0f, state.adaptiveHighFailingFps),
                                   static_cast<double>(state.adaptiveHighFailingP95Us));
    double fraction = 0.5;
    if (highCost > lowCost && budgetUs > lowCost && budgetUs < highCost) {
      fraction = (lowCost - budgetUs) / (lowCost - highCost);
      fraction = 0.65 * fraction + 0.35 * 0.5;
    }
    fraction = std::clamp(fraction, 0.2, 0.8);
    const auto delta = static_cast<size_t>(
        std::llround(static_cast<double>(high - low) * fraction));
    const auto candidate = low + std::clamp<size_t>(delta, 1, high - low - 1);
    return candidate;
  }
  if (high > 1 && low == 0) {
    const auto cost = std::max(1000000.0 / std::max(1.0f, state.adaptiveWindowFps),
                               static_cast<double>(state.adaptiveWindowP95Us));
    const auto ratio = std::clamp(budgetUs / std::max(1.0, cost), 0.35, 0.9);
    const auto candidate = static_cast<size_t>(
        std::llround(static_cast<double>(high) * ratio));
    return std::clamp<size_t>(candidate, 1, high - 1);
  }
  const auto effectiveCostUs = std::max(
      1000000.0 / std::max(1.0f, state.adaptiveWindowFps),
      static_cast<double>(state.adaptiveWindowP95Us));
  const auto costHeadroom = std::max(0.0, budgetUs / std::max(1.0, effectiveCostUs) - 1.0);
  const auto fraction = std::clamp(0.5 * costHeadroom, 0.02, 0.45);
  const auto delta = std::max<size_t>(
      1, static_cast<size_t>(std::ceil(static_cast<double>(sceneCount) * fraction)));
  return sceneCount > std::numeric_limits<size_t>::max() - delta
      ? std::numeric_limits<size_t>::max() : sceneCount + delta;
}

void TGFXBaseView::startAdaptiveCountTransition(size_t targetCount, int64_t nowUs) {
  targetCount = std::max<size_t>(1, targetCount);
  if (targetCount == sceneCount) {
    return;
  }
  if (targetCount < sceneCount) {
    ++benchmarkState.adaptiveRetractionCount;
  }
  benchmarkState.adaptiveGrowthStartUs = nowUs;
  benchmarkState.adaptiveGrowthFrom = sceneCount;
  benchmarkState.adaptiveGrowthTo = targetCount;
}

void TGFXBaseView::updateAdaptiveBenchmark(int64_t nowUs) {
  auto& state = benchmarkState;
  if (!state.active || !state.adaptive ||
      (state.adaptiveSettled && !state.adaptiveTargetReached) ||
      !state.measurementStarted) {
    return;
  }
  if (state.adaptiveRampStartUs == 0) {
    state.adaptiveRampStartUs = nowUs;
  }
  if (state.adaptiveGrowthStartUs > 0) {
    const auto progress = std::min(1.0, (nowUs - state.adaptiveGrowthStartUs) / 200000.0);
    size_t requested = state.adaptiveGrowthTo;
    if (progress < 1.0) {
      if (state.adaptiveGrowthFrom <= state.adaptiveGrowthTo) {
        requested = state.adaptiveGrowthFrom + static_cast<size_t>(
            (state.adaptiveGrowthTo - state.adaptiveGrowthFrom) * progress);
      } else {
        requested = state.adaptiveGrowthFrom - static_cast<size_t>(
            (state.adaptiveGrowthFrom - state.adaptiveGrowthTo) * progress);
      }
    }
    applyBenchmarkSceneCount(requested);
    state.adaptivePeakCount = std::max(state.adaptivePeakCount, sceneCount);
    if (progress >= 1.0) {
      state.adaptiveStalledDecisions = sceneCount == state.adaptiveGrowthFrom
          ? state.adaptiveStalledDecisions + 1 : 0;
      state.adaptiveGrowthStartUs = 0;
      if (state.adaptiveStalledDecisions >= 3) {
        state.adaptiveSettled = true;
        state.adaptiveTargetReached = false;
        state.adaptiveCapacityLimited = true;
        state.adaptiveSettledAtUs = nowUs;
        state.adaptiveSettledCount = sceneCount;
      }
    }
    return;
  }
  float fps = 0;
  int64_t p95IntervalUs = 0;
  if (!getAdaptiveWindowStats(fps, p95IntervalUs)) {
    return;
  }
  const auto tolerance = std::max(0.5f, state.adaptiveTargetFps * 0.01f);
  const auto targetCeilingFps = state.adaptiveTargetFps * 1.25f;
  const auto budgetUs = static_cast<int64_t>(1000000.0 / state.adaptiveTargetFps);
  const auto p95ToleranceUs =
      std::max<int64_t>(1000, static_cast<int64_t>(budgetUs * 0.05));
  const bool passing = fps >= state.adaptiveTargetFps - tolerance &&
                       p95IntervalUs <= budgetUs + p95ToleranceUs;
  state.adaptiveLastDecisionUs = nowUs;
  state.adaptiveWindowFps = fps;
  state.adaptiveWindowP95Us = p95IntervalUs;
  state.adaptiveWindowPassing = passing;
  ++state.adaptiveSearchIteration;

  if (state.adaptiveQuick) {
    if (passing) {
      state.adaptiveConsecutiveFailures = 0;
      state.adaptiveLastPassingCount = sceneCount;
      state.adaptiveLowPassingCount = sceneCount;
      state.adaptiveLowPassingFps = fps;
      state.adaptiveLowPassingP95Us = p95IntervalUs;
      startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
      return;
    }
    state.adaptiveHighFailingCount = sceneCount;
    state.adaptiveHighFailingFps = fps;
    state.adaptiveHighFailingP95Us = p95IntervalUs;
    ++state.adaptiveConsecutiveFailures;
    if (state.adaptiveConsecutiveFailures >= 2) {
      state.adaptiveConsecutiveFailures = 0;
      state.adaptiveSettled = true;
      state.adaptiveTargetReached = false;
      state.adaptiveThresholdReached = true;
      state.adaptiveSettledAtUs = nowUs;
      state.adaptiveSettledCount = sceneCount;
    }
    return;
  }

  if (!state.adaptiveStabilizing) {
    if (passing) {
      state.adaptiveConsecutiveFailures = 0;
      state.adaptiveLastPassingCount = sceneCount;
      state.adaptiveLowPassingCount = sceneCount;
      state.adaptiveLowPassingFps = fps;
      state.adaptiveLowPassingP95Us = p95IntervalUs;
      startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
      return;
    }
    ++state.adaptiveConsecutiveFailures;
    if (state.adaptiveConsecutiveFailures < 2) {
      return;
    }
    state.adaptiveStabilizing = true;
    state.adaptiveStableStartUs = 0;
    state.adaptiveConsecutiveFailures = 0;
    state.adaptiveHighFailingCount = sceneCount;
    state.adaptiveHighFailingFps = fps;
    state.adaptiveHighFailingP95Us = p95IntervalUs;
    startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
    return;
  }

  if (state.adaptiveSettled && state.adaptiveTargetReached) {
    if (passing) {
      state.adaptiveConsecutiveFailures = 0;
      return;
    }
    state.adaptiveStableStartUs = 0;
    state.adaptiveSettled = false;
    state.adaptiveTargetReached = false;
    state.adaptiveSettledCount = 0;
    state.adaptiveLowPassingCount = 0;
    state.adaptiveHighFailingCount = sceneCount;
    state.adaptiveHighFailingFps = fps;
    state.adaptiveHighFailingP95Us = p95IntervalUs;
    ++state.adaptiveStabilityLossCount;
    ++state.adaptiveConsecutiveFailures;
    if (state.adaptiveConsecutiveFailures >= 2) {
      state.adaptiveConsecutiveFailures = 0;
      startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
    }
    return;
  }

  if (passing) {
    state.adaptiveConsecutiveFailures = 0;
    state.adaptiveLowPassingCount = sceneCount;
    state.adaptiveLowPassingFps = fps;
    state.adaptiveLowPassingP95Us = p95IntervalUs;
    state.adaptiveLastPassingCount = sceneCount;
    if (fps > targetCeilingFps &&
        (state.adaptiveHighFailingCount == 0 ||
         state.adaptiveHighFailingFps > targetCeilingFps)) {
      state.adaptiveHighFailingCount = 0;
      state.adaptiveStabilizing = false;
      state.adaptiveStableStartUs = 0;
      startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
      return;
    }
    if (state.adaptiveHighFailingCount <= sceneCount) {
      // A recovered boundary is no longer a failing upper bound; search upward again.
      state.adaptiveHighFailingCount = 0;
      state.adaptiveStabilizing = false;
      state.adaptiveStableStartUs = 0;
      startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
      return;
    }
    const auto high = state.adaptiveHighFailingCount;
    const auto bracketTolerance =
        high > 0 ? std::max<size_t>(1, high / 20) : 0;
    if (high > state.adaptiveLowPassingCount &&
        high - state.adaptiveLowPassingCount > bracketTolerance) {
      state.adaptiveStableStartUs = 0;
      startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
      return;
    }
    if (state.adaptiveStableStartUs == 0) {
      state.adaptiveStableStartUs = nowUs;
    }
    if (nowUs - state.adaptiveStableStartUs >= state.adaptiveStableWindowUs) {
      state.adaptiveSettled = true;
      state.adaptiveTargetReached = true;
      state.adaptiveSettledAtUs = nowUs;
      state.adaptiveSettledCount = sceneCount;
    }
    return;
  }

  state.adaptiveStableStartUs = 0;
  if (fps > targetCeilingFps) {
    // A p95-only failure while average FPS is far above target is not a useful
    // capacity ceiling. Keep growing until both metrics approach the budget.
    state.adaptiveHighFailingCount = 0;
    state.adaptiveStabilizing = false;
    state.adaptiveConsecutiveFailures = 0;
    startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
    return;
  }
  const auto contradictedPassingBound =
      state.adaptiveLowPassingCount >= sceneCount && state.adaptiveLowPassingCount > 0;
  if (contradictedPassingBound) {
    // The same count cannot be both a verified pass and a fail. Alternating windows
    // indicate that this count sits on a noisy boundary, so probe below it again.
    state.adaptiveLowPassingCount = 0;
    state.adaptiveConsecutiveFailures = 1;
  }
  state.adaptiveHighFailingCount = sceneCount;
  state.adaptiveHighFailingFps = fps;
  state.adaptiveHighFailingP95Us = p95IntervalUs;
  ++state.adaptiveConsecutiveFailures;
  if (state.adaptiveConsecutiveFailures < 2) {
    return;
  }
  state.adaptiveConsecutiveFailures = 0;
  if (sceneCount == 1) {
    state.adaptiveSettled = true;
    state.adaptiveTargetReached = false;
    state.adaptiveSettledAtUs = nowUs;
    state.adaptiveSettledCount = sceneCount;
    return;
  }
  if (state.adaptiveLowPassingCount > 0 &&
      sceneCount <= state.adaptiveLowPassingCount + 1) {
    startAdaptiveCountTransition(state.adaptiveLowPassingCount, nowUs);
    return;
  }
  startAdaptiveCountTransition(nextAdaptiveCandidateCount(), nowUs);
}

void TGFXBaseView::stopBenchmarkRun() {
  if (!benchmarkState.active) {
    return;
  }
  if (benchmarkState.adaptiveTargetReached) {
    std::vector<int64_t> intervals;
    int64_t previousUs = 0;
    int64_t totalUs = 0;
    const auto size = benchmarkState.samples.size();
    const auto first = benchmarkState.overflowed ? benchmarkState.recordedFrameCount % size : 0;
    for (size_t i = 0; i < size; ++i) {
      const auto& sample = benchmarkState.samples[(first + i) % size];
      if (sample.callbackBeginUs < benchmarkState.adaptiveStableStartUs ||
          sample.sceneCount != benchmarkState.adaptiveSettledCount) {
        previousUs = 0;
        continue;
      }
      if (previousUs > 0) {
        intervals.push_back(sample.callbackBeginUs - previousUs);
        totalUs += intervals.back();
      }
      previousUs = sample.callbackBeginUs;
    }
    std::sort(intervals.begin(), intervals.end());
    const auto budgetUs = 1000000.0 / benchmarkState.adaptiveTargetFps;
    const auto p95Index = intervals.empty() ? 0 :
        static_cast<size_t>(std::ceil(intervals.size() * 0.95)) - 1;
    const auto fps = totalUs > 0 ? intervals.size() * 1000000.0 / totalUs : 0;
    if (intervals.empty() ||
        fps < benchmarkState.adaptiveTargetFps -
                  std::max(0.5f, benchmarkState.adaptiveTargetFps * 0.01f) ||
        intervals[p95Index] > budgetUs + std::max(1000.0, budgetUs * 0.05)) {
      benchmarkState.adaptiveTargetReached = false;
      benchmarkState.adaptiveSettled = false;
      benchmarkState.adaptiveStableStartUs = 0;
      ++benchmarkState.adaptiveStabilityLossCount;
    }
  }
  if (window != nullptr) {
    auto device = window->getDevice();
    if (device != nullptr) {
      auto context = device->lockContext();
      if (context != nullptr) {
        if (lastRecording != nullptr) {
          context->submit(std::move(lastRecording));
          lastRecording = nullptr;
        }
        device->unlock();
      }
    }
  }
  const auto stopUs = tgfx::Clock::Now();
  benchmarkState.stopUs = stopUs;
  if (benchmarkState.manual || benchmarkState.coldStart) {
    benchmarkState.measurementEndUs = stopUs;
  }
  benchmarkState.drainAfterMeasurementUs = stopUs - benchmarkState.measurementEndUs;
  benchmarkState.active = false;
  benchmarkState.complete = true;

  auto jsWindow = emscripten::val::global("window");
  if (jsWindow.hasOwnProperty("__tgfxBenchmarkRunComplete")) {
    jsWindow.call<void>("__tgfxBenchmarkRunComplete", getBenchmarkSummaryJson());
  }
}

bool TGFXBaseView::isBenchmarkComplete() const {
  return benchmarkState.complete;
}

std::string TGFXBaseView::getBenchmarkSummaryJson() const {
  std::ostringstream json;
  json << "{";
  json << "\"schemaVersion\":\"tgfx-benchmark.summary.v2\"";
  json << ",\"executionStatus\":\"" << (benchmarkState.complete ? "completed" : "running") << "\"";
  json << ",\"testIntent\":\""
       << (benchmarkState.coldStart ? "cold-start" : "steady-state") << "\"";
  json << ",\"coldStart\":" << (benchmarkState.coldStart ? "true" : "false");
  json << ",\"measurementStarted\":"
       << (benchmarkState.measurementStarted ? "true" : "false");
  json << ",\"manual\":" << (benchmarkState.manual ? "true" : "false");
  json << ",\"adaptive\":" << (benchmarkState.adaptive ? "true" : "false");
  json << ",\"adaptiveQuick\":" << (benchmarkState.adaptiveQuick ? "true" : "false");
  json << ",\"adaptiveSettled\":" << (benchmarkState.adaptiveSettled ? "true" : "false");
  json << ",\"adaptiveTargetReached\":"
       << (benchmarkState.adaptiveTargetReached ? "true" : "false");
  json << ",\"adaptiveThresholdReached\":"
       << (benchmarkState.adaptiveThresholdReached ? "true" : "false");
  json << ",\"adaptiveCapacityLimited\":"
       << (benchmarkState.adaptiveCapacityLimited ? "true" : "false");
  json << ",\"adaptiveRetractionCount\":" << benchmarkState.adaptiveRetractionCount;
  json << ",\"adaptiveStabilityLossCount\":" << benchmarkState.adaptiveStabilityLossCount;
  json << ",\"adaptiveWindowCount\":" << benchmarkState.adaptiveWindowCount;
  json << ",\"adaptiveWindowFps\":" << benchmarkState.adaptiveWindowFps;
  json << ",\"adaptiveWindowP95Us\":" << benchmarkState.adaptiveWindowP95Us;
  json << ",\"adaptiveWindowPassing\":"
       << (benchmarkState.adaptiveWindowPassing ? "true" : "false");
  json << ",\"adaptiveTargetFps\":" << benchmarkState.adaptiveTargetFps;
  json << ",\"adaptiveStableWindowUs\":" << benchmarkState.adaptiveStableWindowUs;
  json << ",\"adaptiveInitialCount\":" << benchmarkState.adaptiveInitialCount;
  json << ",\"adaptivePeakCount\":" << benchmarkState.adaptivePeakCount;
  json << ",\"adaptiveLowPassingCount\":" << benchmarkState.adaptiveLowPassingCount;
  json << ",\"adaptiveHighFailingCount\":" << benchmarkState.adaptiveHighFailingCount;
  json << ",\"adaptiveLowPassingFps\":" << benchmarkState.adaptiveLowPassingFps;
  json << ",\"adaptiveHighFailingFps\":" << benchmarkState.adaptiveHighFailingFps;
  json << ",\"adaptiveSearchIteration\":" << benchmarkState.adaptiveSearchIteration;
  json << ",\"adaptiveSettledCount\":" << benchmarkState.adaptiveSettledCount;
  json << ",\"finalSceneCount\":" << sceneCount;
  json << ",\"adaptiveStableStartUs\":" << benchmarkState.adaptiveStableStartUs;
  json << ",\"adaptiveRampDurationUs\":"
       << (benchmarkState.adaptiveSettledAtUs > benchmarkState.adaptiveRampStartUs
               ? benchmarkState.adaptiveSettledAtUs - benchmarkState.adaptiveRampStartUs
               : 0);
  json << ",\"stopReason\":\""
       << (benchmarkState.coldStart
               ? "cold-start-window"
               : benchmarkState.adaptive ? (benchmarkState.adaptiveQuick &&
                                          benchmarkState.adaptiveThresholdReached
                                      ? "adaptive-threshold-user-stop"
                                      : benchmarkState.adaptiveTargetReached
                                          ? "adaptive-settled-user-stop"
                                          : "adaptive-unsettled-user-stop")
           : benchmarkState.manual ? "user-stop" : "measurement-window")
       << "\"";
  const auto warmupDurationUs =
      benchmarkState.measurementStartUs > benchmarkState.startUs
          ? benchmarkState.measurementStartUs - benchmarkState.startUs
          : 0;
  json << ",\"warmupDurationUs\":" << warmupDurationUs;
  json << ",\"warmupFrameCount\":" << benchmarkState.steadyWarmupFrameCount;
  json << ",\"measurementDurationUs\":"
       << std::max<int64_t>(0, benchmarkState.measurementEndUs -
                                  benchmarkState.measurementStartUs);
  json << ",\"coldStartInitialWindowFrames\":"
       << benchmarkState.coldStartInitialWindowFrames;
  json << ",\"coldStartFramesAfterVisible\":"
       << benchmarkState.coldStartFramesAfterVisible;
  json << ",\"coldStartFirstVisibleUs\":" << benchmarkState.coldStartFirstVisibleUs;
  json << ",\"sampleCount\":" << benchmarkState.samples.size();
  json << ",\"overflowed\":" << (benchmarkState.overflowed ? "true" : "false");
  json << ",\"recordedFrameCount\":" << benchmarkState.recordedFrameCount;
  json << ",\"drawnFrameCount\":" << benchmarkState.nextFrameId - 1;
  json << ",\"firstSubmitUs\":" << benchmarkState.firstSubmitUs;
  const auto firstFrameAvailable =
      benchmarkState.coldStart && !benchmarkState.samples.empty() && !benchmarkState.overflowed;
  json << ",\"firstFrame\":{";
  json << "\"available\":" << (firstFrameAvailable ? "true" : "false");
  if (firstFrameAvailable) {
    const auto& firstFrame = benchmarkState.samples.front();
    json << ",\"reason\":null";
    json << ",\"callbackUs\":" << firstFrame.callbackEndUs - firstFrame.callbackBeginUs;
    json << ",\"sceneDrawUs\":" << firstFrame.sceneDrawUs;
    json << ",\"flushUs\":" << firstFrame.flushUs;
    json << ",\"submitUs\":" << firstFrame.submitUs;
  } else {
    const auto reason = !benchmarkState.coldStart
        ? "not-cold-start"
        : benchmarkState.samples.empty() ? "no-sample" : "sample-overflowed";
    json << ",\"reason\":\"" << reason << "\"";
    json << ",\"callbackUs\":null";
    json << ",\"sceneDrawUs\":null";
    json << ",\"flushUs\":null";
    json << ",\"submitUs\":null";
  }
  json << "}";
  json << ",\"drainAfterMeasurementUs\":" << benchmarkState.drainAfterMeasurementUs;
  json << ",\"samples\":[";
  const auto size = benchmarkState.samples.size();
  const auto first = benchmarkState.overflowed ? benchmarkState.recordedFrameCount % size : 0;
  for (size_t i = 0; i < size; ++i) {
    const auto& sample = benchmarkState.samples[(first + i) % size];
    if (i != 0) {
      json << ",";
    }
    json << "{"
         << "\"frameId\":" << sample.frameId
         << ",\"recordingId\":" << sample.recordingId
         << ",\"submittedFrameId\":" << sample.submittedFrameId
         << ",\"callbackBeginUs\":" << sample.callbackBeginUs
         << ",\"callbackEndUs\":" << sample.callbackEndUs
         << ",\"sceneDrawUs\":" << sample.sceneDrawUs
         << ",\"flushUs\":" << sample.flushUs
         << ",\"submitUs\":" << sample.submitUs
         << ",\"sceneCount\":" << sample.sceneCount
         << "}";
  }
  json << "]}";
  return json.str();
}

std::string TGFXBaseView::getBenchmarkLiveStatsJson() const {
  const auto nowUs = tgfx::Clock::Now();
  const auto warmupDurationUs =
      benchmarkState.measurementStartUs > benchmarkState.startUs
          ? benchmarkState.measurementStartUs - benchmarkState.startUs
          : 0;
  const auto elapsedUs = benchmarkState.startUs ? nowUs - benchmarkState.startUs : 0;
  const auto warmupElapsedUs = benchmarkState.measurementStarted
      ? warmupDurationUs : elapsedUs;
  const auto measurementElapsedUs = benchmarkState.measurementStarted
      ? std::max<int64_t>(0, nowUs - benchmarkState.measurementStartUs) : 0;
  const auto size = benchmarkState.samples.size();
  const auto* lastSample = size == 0 ? nullptr
      : &benchmarkState.samples[(benchmarkState.recordedFrameCount - 1) % size];
  const auto* previousSample = size < 2 ? nullptr
      : &benchmarkState.samples[(benchmarkState.recordedFrameCount - 2) % size];

  std::ostringstream json;
  json << "{";
  json << "\"schemaVersion\":\"tgfx-benchmark.live.v2\"";
  json << ",\"drawnFrameCount\":" << benchmarkState.nextFrameId - 1;
  json << ",\"active\":" << (benchmarkState.active ? "true" : "false");
  json << ",\"complete\":" << (benchmarkState.complete ? "true" : "false");
  json << ",\"testIntent\":\""
       << (benchmarkState.coldStart ? "cold-start" : "steady-state") << "\"";
  json << ",\"coldStart\":" << (benchmarkState.coldStart ? "true" : "false");
  json << ",\"measurementStarted\":"
       << (benchmarkState.measurementStarted ? "true" : "false");
  json << ",\"successfulFrameCount\":" << benchmarkState.successfulFrameCount;
  json << ",\"warmupFrameCount\":" << benchmarkState.steadyWarmupFrameCount;
  json << ",\"manual\":" << (benchmarkState.manual ? "true" : "false");
  json << ",\"adaptive\":" << (benchmarkState.adaptive ? "true" : "false");
  json << ",\"adaptiveQuick\":" << (benchmarkState.adaptiveQuick ? "true" : "false");
  json << ",\"adaptiveSettled\":" << (benchmarkState.adaptiveSettled ? "true" : "false");
  json << ",\"adaptiveTargetReached\":"
       << (benchmarkState.adaptiveTargetReached ? "true" : "false");
  json << ",\"adaptiveThresholdReached\":"
       << (benchmarkState.adaptiveThresholdReached ? "true" : "false");
  json << ",\"adaptiveCapacityLimited\":"
       << (benchmarkState.adaptiveCapacityLimited ? "true" : "false");
  json << ",\"adaptiveRetractionCount\":" << benchmarkState.adaptiveRetractionCount;
  json << ",\"adaptiveStabilityLossCount\":" << benchmarkState.adaptiveStabilityLossCount;
  json << ",\"adaptiveWindowCount\":" << benchmarkState.adaptiveWindowCount;
  json << ",\"adaptiveWindowFps\":" << benchmarkState.adaptiveWindowFps;
  json << ",\"adaptiveWindowP95Us\":" << benchmarkState.adaptiveWindowP95Us;
  json << ",\"adaptiveWindowPassing\":"
       << (benchmarkState.adaptiveWindowPassing ? "true" : "false");
  json << ",\"imageAvailable\":" << (appHost->getImage("bridge") ? "true" : "false");
  json << ",\"imageCount\":" << appHost->imageCount();
  json << ",\"adaptiveStabilizing\":"
       << (benchmarkState.adaptiveStabilizing ? "true" : "false");
  json << ",\"adaptiveTargetFps\":" << benchmarkState.adaptiveTargetFps;
  json << ",\"adaptiveInitialCount\":" << benchmarkState.adaptiveInitialCount;
  json << ",\"adaptivePeakCount\":" << benchmarkState.adaptivePeakCount;
  json << ",\"adaptiveLowPassingCount\":" << benchmarkState.adaptiveLowPassingCount;
  json << ",\"adaptiveHighFailingCount\":" << benchmarkState.adaptiveHighFailingCount;
  json << ",\"adaptiveLowPassingFps\":" << benchmarkState.adaptiveLowPassingFps;
  json << ",\"adaptiveHighFailingFps\":" << benchmarkState.adaptiveHighFailingFps;
  json << ",\"adaptiveSearchIteration\":" << benchmarkState.adaptiveSearchIteration;
  json << ",\"adaptiveSettledCount\":" << benchmarkState.adaptiveSettledCount;
  json << ",\"finalSceneCount\":" << sceneCount;
  json << ",\"adaptiveStableStartUs\":" << benchmarkState.adaptiveStableStartUs;
  json << ",\"adaptiveRampElapsedUs\":"
       << (benchmarkState.adaptiveRampStartUs > 0
               ? nowUs - benchmarkState.adaptiveRampStartUs
               : 0);
  json << ",\"elapsedUs\":" << elapsedUs;
  json << ",\"warmupElapsedUs\":" << warmupElapsedUs;
  json << ",\"warmupDurationUs\":" << warmupDurationUs;
  json << ",\"measurementElapsedUs\":" << measurementElapsedUs;
  json << ",\"coldStartInitialWindowFrames\":"
       << benchmarkState.coldStartInitialWindowFrames;
  json << ",\"coldStartFramesAfterVisible\":"
       << benchmarkState.coldStartFramesAfterVisible;
  json << ",\"coldStartFirstVisibleUs\":" << benchmarkState.coldStartFirstVisibleUs;
  json << ",\"sampleCount\":" << benchmarkState.samples.size();
  json << ",\"overflowed\":" << (benchmarkState.overflowed ? "true" : "false");
  json << ",\"recordedFrameCount\":" << benchmarkState.recordedFrameCount;
  json << ",\"currentFps\":" << appHost->currentFPS();
  json << ",\"sceneCount\":" << sceneCount;
  json << ",\"mouseX\":" << appHost->mouseX();
  json << ",\"mouseY\":" << appHost->mouseY();
  json << ",\"frameIntervalMs\":"
       << (lastFrameIntervalUs > 0 ? static_cast<double>(lastFrameIntervalUs) / 1000.0 : 0.0);
  json << ",\"frameCallbackMs\":" << static_cast<double>(lastFrameCallbackUs) / 1000.0;
  if (lastSample != nullptr) {
    json << ",\"lastCallbackMs\":"
         << static_cast<double>(lastSample->callbackEndUs - lastSample->callbackBeginUs) / 1000.0;
    json << ",\"lastSceneDrawMs\":" << static_cast<double>(lastSample->sceneDrawUs) / 1000.0;
    json << ",\"lastFlushMs\":" << static_cast<double>(lastSample->flushUs) / 1000.0;
    json << ",\"lastSubmitMs\":" << static_cast<double>(lastSample->submitUs) / 1000.0;
  } else {
    json << ",\"lastCallbackMs\":null,\"lastSceneDrawMs\":null"
         << ",\"lastFlushMs\":null,\"lastSubmitMs\":null";
  }
  if (lastSample != nullptr && previousSample != nullptr) {
    json << ",\"lastIntervalMs\":"
         << static_cast<double>(lastSample->callbackBeginUs - previousSample->callbackBeginUs) /
                1000.0;
  } else {
    json << ",\"lastIntervalMs\":null";
  }
  json << "}";
  return json.str();
}

Bench* TGFXBaseView::getBenchByIndex() const {
  const auto numBenches = benchmark::Bench::Count();
  const auto index = (drawIndex % numBenches);
  return benchmark::Bench::GetByIndex(index);
}

void TGFXBaseView::showPerfData(bool show) {
  showPerfDataFlag = show;
}

void TGFXBaseView::setAntiAlias(bool aa) {
  ParticleBench::SetAntiAlias(aa);
  appHost->resetFrames();
}

void TGFXBaseView::setStroke(bool stroke) {
  ParticleBench::SetStroke(stroke);
  appHost->resetFrames();
}

emscripten::val TGFXBaseView::getAOTEvidence() const {
  auto result = emscripten::val::object();
#if !TGFX_BENCHMARK_HAS_AOT_CACHE
  result.set("available", false);
  result.set("reason", "precompiled-shader-cache-not-available");
  return result;
#else
  auto device = window ? window->getDevice() : nullptr;
  if (device == nullptr) {
    result.set("available", false);
    return result;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    result.set("available", false);
    return result;
  }
  auto* cache = context->precompiledShaderCache();
  result.set("available", true);
  result.set("isLoaded", cache->isLoaded());
  result.set("bundleState", static_cast<int>(cache->embeddedBundleState()));
  result.set("vertexEntries", cache->vertexEntryCount());
  result.set("fragmentEntries", cache->fragmentEntryCount());
  result.set("profile", cache->profileTag());
  result.set("identityHash", static_cast<double>(cache->bundleIdentityHash()));
  result.set("identityHashHex", HexValue(cache->bundleIdentityHash(), 16));
  result.set("toolchainABI", cache->bundleToolchainABI());
  result.set("toolchainABIHex", HexValue(cache->bundleToolchainABI(), 8));
  result.set("hitCount", cache->hitCount());
  result.set("missCount", cache->missCount());
  auto hitSummary = emscripten::val::array();
  std::map<std::string, uint32_t> shaderHits = {};
  for (const auto& record : cache->hitRecords()) {
    ++shaderHits[record.shaderName];
  }
  for (const auto& [shaderName, count] : shaderHits) {
    auto entry = emscripten::val::object();
    entry.set("shader", shaderName);
    entry.set("count", count);
    hitSummary.call<void>("push", entry);
  }
  result.set("hitSummary", hitSummary);
  struct FallbackSummaryEntry {
    std::string reason = {};
    std::string shaderName = {};
    std::string effectSignature = {};
    std::string pipelineSignature = {};
    uint32_t matchFailure = 0;
    uint32_t count = 0;
  };
  std::map<std::string, FallbackSummaryEntry> fallbackSummaryMap = {};
  for (const auto& record : cache->fallbackRecords()) {
    auto reason = std::string(PrecompiledFallbackReasonName(record.reason));
    auto key = reason + "\n" + std::to_string(record.matchFailure) + "\n" +
               record.shaderName + "\n" + record.effectSignature + "\n" +
               record.pipelineSignature;
    auto& entry = fallbackSummaryMap[key];
    entry.reason = reason;
    entry.shaderName = record.shaderName;
    entry.effectSignature = record.effectSignature;
    entry.pipelineSignature = record.pipelineSignature;
    entry.matchFailure = record.matchFailure;
    ++entry.count;
  }
  auto fallbackSummary = emscripten::val::array();
  for (const auto& [key, summary] : fallbackSummaryMap) {
    (void)key;
    auto entry = emscripten::val::object();
    entry.set("reason", summary.reason);
    entry.set("shader", summary.shaderName);
    entry.set("effect", summary.effectSignature);
    entry.set("pipeline", summary.pipelineSignature);
    entry.set("matchFailure", summary.matchFailure);
    entry.set("count", summary.count);
    fallbackSummary.call<void>("push", entry);
  }
  result.set("fallbackSummary", fallbackSummary);
  auto failureCount = 0u;
  auto fallbackCounts = emscripten::val::object();
  for (size_t index = 0; index < static_cast<size_t>(tgfx::PrecompiledFallbackReason::Count);
       ++index) {
    auto reason = static_cast<tgfx::PrecompiledFallbackReason>(index);
    auto count = cache->fallbackCount(reason);
    if (count > 0) {
      failureCount += count;
      fallbackCounts.set(tgfx::PrecompiledFallbackReasonName(reason), count);
    }
  }
  result.set("failureCount", failureCount);
  result.set("fallbackCounts", fallbackCounts);
  result.set("decompositionEnabled", cache->decompositionEnabled());
  device->unlock();
  return result;
#endif
}

}  // namespace benchmark

int main() {
  return 0;
}
