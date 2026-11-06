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

#pragma once

#include <emscripten/bind.h>
#include <vector>
#include "base/Bench.h"
#include "base/AppHost.h"
#include "benchmark/ParticleBench.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Window.h"
namespace benchmark {

class TGFXBaseView {
 public:
  TGFXBaseView(const std::string& canvasID);

  void setImagePath(const std::string& imagePath);

  bool setImageSource(tgfx::NativeImageRef nativeImage);

  bool setNamedImageSource(const std::string& name, tgfx::NativeImageRef nativeImage);

  void updateSize(float devicePixelRatio);

  void startDraw();

  void draw();

  void restartDraw() const;

  void updatePerfInfo(const PerfData& data);

  void updateDrawParam(const DrawParam& drawParam) const;

  void updateGraphicType(int type);

  bool setSceneById(const std::string& stableId);

  void setSceneCount(size_t count);

  void setRecorderEnabled(bool enabled);

  bool startBenchmarkRun(int64_t measurementUs);

  bool startManualBenchmarkRun();

  bool startAdaptiveBenchmarkRun(int64_t stableWindowUs, float targetFps);

  bool startColdStartBenchmarkRun(int32_t initialWindowFrames);

  void markColdStartVisible(int64_t elapsedUs);

  void stopBenchmarkRun();

  bool isBenchmarkComplete() const;

  std::string getBenchmarkSummaryJson() const;

  std::string getBenchmarkLiveStatsJson() const;

  Bench* getBenchByIndex() const;

  void showPerfData(bool show);

  void setAntiAlias(bool aa);

  void setStroke(bool stroke);

  emscripten::val getAOTEvidence() const;

  int drawIndex = 0;
  std::unique_ptr<tgfx::Recording> lastRecording = nullptr;
  std::shared_ptr<benchmark::AppHost> appHost = nullptr;
  bool showPerfDataFlag = true;
  bool firstDrawNotified = false;
  bool recorderEnabled = true;
  int64_t lastFrameIntervalUs = 0;
  int64_t lastFrameBeginUs = 0;
  int64_t lastFrameCallbackUs = 0;
  size_t sceneCount = 0;
  float mouseScaleX = 1.0f;
  float mouseScaleY = 1.0f;

  struct FrameSample {
    uint64_t frameId;
    uint64_t recordingId;
    uint64_t submittedFrameId;
    int64_t callbackBeginUs;
    int64_t callbackEndUs;
    int64_t sceneDrawUs;
    int64_t flushUs;
    int64_t submitUs;
    size_t sceneCount;
  };

  struct BenchmarkState {
    bool active = false;
    bool complete = false;
    bool overflowed = false;
    bool manual = false;
    bool adaptive = false;
    bool coldStart = false;
    bool measurementStarted = false;
    bool adaptiveQuick = false;
    bool adaptiveSettled = false;
    bool adaptiveTargetReached = false;
    bool adaptiveThresholdReached = false;
    bool adaptiveCapacityLimited = false;
    bool adaptiveStabilizing = false;
    float adaptiveTargetFps = 60.0f;
    int64_t adaptiveStableWindowUs = 0;
    int64_t adaptiveRampStartUs = 0;
    int64_t adaptiveSettledAtUs = 0;
    int64_t adaptiveStableStartUs = 0;
    int64_t adaptiveLastDecisionUs = 0;
    size_t adaptiveInitialCount = 0;
    size_t adaptivePeakCount = 0;
    size_t adaptiveLastPassingCount = 0;
    size_t adaptiveLowPassingCount = 0;
    size_t adaptiveHighFailingCount = 0;
    float adaptiveLowPassingFps = 0.0f;
    float adaptiveHighFailingFps = 0.0f;
    int64_t adaptiveLowPassingP95Us = 0;
    int64_t adaptiveHighFailingP95Us = 0;
    uint32_t adaptiveSearchIteration = 0;
    size_t adaptiveSettledCount = 0;
    uint32_t adaptiveStalledDecisions = 0;
    uint32_t adaptiveRetractionCount = 0;
    uint32_t adaptiveConsecutiveFailures = 0;
    uint32_t adaptiveStabilityLossCount = 0;
    uint64_t adaptiveLastEvaluatedFrameId = 0;
    int64_t adaptiveCountChangedUs = 0;
    uint32_t adaptiveWindowCount = 0;
    float adaptiveWindowFps = 0;
    int64_t adaptiveWindowP95Us = 0;
    bool adaptiveWindowPassing = false;
    int64_t adaptiveGrowthStartUs = 0;
    size_t adaptiveGrowthFrom = 0;
    size_t adaptiveGrowthTo = 0;
    uint64_t recordedFrameCount = 0;
    uint64_t successfulFrameCount = 0;
    uint64_t steadyWarmupFrameCount = 0;
    uint64_t coldStartVisibleFrameId = 0;
    uint64_t coldStartFramesAfterVisible = 0;
    int32_t coldStartInitialWindowFrames = 30;
    int64_t startUs = 0;
    int64_t coldStartFirstVisibleUs = 0;
    int64_t measurementStartUs = 0;
    int64_t measurementEndUs = 0;
    int64_t requestedMeasurementUs = 0;
    int64_t stopUs = 0;
    int64_t firstSubmitUs = 0;
    int64_t drainAfterMeasurementUs = 0;
    uint64_t nextFrameId = 1;
    uint64_t lastRecordingFrameId = 0;
    std::vector<FrameSample> samples = {};
  };

  BenchmarkState benchmarkState = {};

 private:
  // Backend-specific factory. Implemented in its own per-backend source file
  // (WebGLWindow.cpp / WebGPUWindow.cpp), selected by CMake according to BENCHMARK_BACKEND.
  static std::shared_ptr<tgfx::Window> MakeTGFXWindow(const std::string& canvasID);

  void applyBenchmarkSceneCount(size_t count);

  void updateAdaptiveBenchmark(int64_t nowUs);

  size_t nextAdaptiveCandidateCount() const;

  void startAdaptiveCountTransition(size_t count, int64_t nowUs);

  bool getAdaptiveWindowStats(float& fps, int64_t& p95IntervalUs);

  std::shared_ptr<tgfx::Window> window = nullptr;
  std::shared_ptr<tgfx::Surface> surface = nullptr;
  std::string canvasID = "";
  Bench* benchmarkBench = nullptr;
};

}  // namespace benchmark
