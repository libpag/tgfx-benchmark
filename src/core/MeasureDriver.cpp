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

#include "core/MeasureDriver.h"
#include <algorithm>
#include <vector>
#include "core/Checkpoint.h"
#include "core/FrameTimer.h"
#include "core/report/SystemInfo.h"
#include "tgfx/platform/Print.h"

namespace benchmark {
namespace {

constexpr int MemorySampleInterval = 10;
constexpr int BreakdownFrameLimit = 60;

/// Throttling and backpressure show up as slow blocks rather than slow individual frames, so the
/// window is also summarized in blocks. Blocks give the comparison tool many more samples per run
/// than one value per repeat.
constexpr int ThroughputBlockFrames = 30;

const tgfx::Color BackgroundColor = {0.87f, 0.87f, 0.87f, 1.0f};

bool IsCheckpointFrame(const std::vector<int64_t>& frames, int64_t frame) {
  for (auto value : frames) {
    if (value == frame) {
      return true;
    }
  }
  return false;
}

double Median(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  auto middle = values.size() / 2;
  if (values.size() % 2 == 0) {
    return (values[middle - 1] + values[middle]) * 0.5;
  }
  return values[middle];
}

}  // namespace

CaseResult MeasureDriver::Run(Scene* scene, Tier tier, const ParamSet& params,
                              const SceneContext& sceneContext, const RunConfig& config,
                              const std::string& checkpointDir) {
  CaseResult result = {};
  result.scene = scene->info().id;
  result.category = scene->info().category;
  result.tier = TierName(tier);
  result.paramsJSON = params.toCanonicalJSON();
  result.paramHash = params.hash();

  auto context = sceneContext.context;
  if (context == nullptr) {
    result.status = "failed";
    result.statusDetail = "no context";
    return result;
  }

  auto surface = tgfx::Surface::Make(context, config.width, config.height);
  if (surface == nullptr) {
    result.status = "failed";
    result.statusDetail = "failed to create offscreen surface";
    return result;
  }

  const auto checkpointFrames = scene->info().checkpointFrames;
  const auto warmupFrames = std::max(config.warmupFrames, 1);
  const auto measureFrames = std::max(config.measureFrames, 1);
  const auto totalFrames = warmupFrames + measureFrames;

  std::vector<double> frameSamples;
  std::vector<double> recordSamples;
  std::vector<double> flushSamples;
  std::vector<double> submitSamples;
  std::vector<double> setupSamples;
  frameSamples.reserve(static_cast<size_t>(totalFrames * config.repeats));

  // Start from an empty cache once so every case begins from the same state.
  context->purgeResourcesUntilMemoryTo(0);

  // Checkpoints force a full readback, which stalls the pipeline. They run in their own pass so
  // the measured samples stay free of those stalls.
  if (!checkpointFrames.empty() && scene->setup(sceneContext, params)) {
    int64_t lastFrame = 0;
    for (auto frame : checkpointFrames) {
      lastFrame = std::max(lastFrame, frame);
    }
    auto caseId = result.scene + "-" + result.tier;
    for (int64_t frame = 0; frame <= lastFrame; frame++) {
      scene->update(frame);
      surface->getCanvas()->clear(BackgroundColor);
      scene->render(surface.get());
      if (IsCheckpointFrame(checkpointFrames, frame)) {
        result.checkpoints.push_back(
            Checkpoint::Capture(surface.get(), checkpointDir, caseId, frame, BackgroundColor));
      }
    }
    context->flushAndSubmit(true);
    scene->teardown();
  }

  for (auto repeat = 0; repeat < config.repeats; repeat++) {
    auto setupStart = FrameTimer::NowMs();
    if (!scene->setup(sceneContext, params)) {
      result.status = "failed";
      result.statusDetail = "scene setup failed";
      return result;
    }
    setupSamples.push_back(FrameTimer::NowMs() - setupStart);

    std::vector<double> repeatFrames;
    repeatFrames.reserve(static_cast<size_t>(measureFrames));
    double measuredStart = 0.0;
    double blockStart = 0.0;
    int blockFrames = 0;

    for (auto frame = 0; frame < totalFrames; frame++) {
      auto recordStart = FrameTimer::NowMs();
      scene->update(frame);
      surface->getCanvas()->clear(BackgroundColor);
      scene->render(surface.get());
      auto recordEnd = FrameTimer::NowMs();
      auto recording = context->flush();
      auto flushEnd = FrameTimer::NowMs();
      if (recording != nullptr) {
        context->submit(std::move(recording));
      }
      auto submitEnd = FrameTimer::NowMs();

      if (frame >= warmupFrames) {
        if (frame == warmupFrames) {
          measuredStart = recordStart;
          blockStart = recordStart;
          blockFrames = 0;
        }
        blockFrames++;
        if (blockFrames == ThroughputBlockFrames) {
          result.throughputBlocks.push_back((submitEnd - blockStart) /
                                            static_cast<double>(blockFrames));
          blockStart = submitEnd;
          blockFrames = 0;
        }
        repeatFrames.push_back(submitEnd - recordStart);
        recordSamples.push_back(recordEnd - recordStart);
        flushSamples.push_back(flushEnd - recordEnd);
        submitSamples.push_back(submitEnd - flushEnd);
      }

      if (frame % MemorySampleInterval == 0) {
        result.gpuCacheBytes =
            std::max(result.gpuCacheBytes, static_cast<uint64_t>(context->memoryUsage()));
        result.footprintBytes = std::max(result.footprintBytes, ProcessFootprintBytes());
      }
    }

    auto drainEnd = [&] {
      context->flushAndSubmit(true);
      return FrameTimer::NowMs();
    }();

    if (measuredStart > 0.0) {
      auto elapsed = drainEnd - measuredStart;
      result.throughputPerRepeat.push_back(elapsed / static_cast<double>(measureFrames));
    }
    frameSamples.insert(frameSamples.end(), repeatFrames.begin(), repeatFrames.end());

    result.gpuCachePeakBytes = std::max(result.gpuCachePeakBytes, result.gpuCacheBytes);
    result.footprintPeakBytes = std::max(result.footprintPeakBytes, result.footprintBytes);

    scene->teardown();
  }

  if (config.breakdownPass) {
    std::vector<double> waitSamples;
    if (scene->setup(sceneContext, params)) {
      const auto frames = std::min(measureFrames, BreakdownFrameLimit);
      for (auto frame = 0; frame < frames; frame++) {
        scene->update(frame);
        surface->getCanvas()->clear(BackgroundColor);
        scene->render(surface.get());
        auto recording = context->flush();
        auto waitStart = FrameTimer::NowMs();
        if (recording != nullptr) {
          context->submit(std::move(recording), true);
        }
        waitSamples.push_back(FrameTimer::NowMs() - waitStart);
      }
      scene->teardown();
      result.gpuMs = SampleStats::Compute(std::move(waitSamples));
      result.gpuSource = "sync_wait";
    }
  }

  result.frameMs = SampleStats::Compute(frameSamples);
  if (config.keepSamples) {
    result.samples = frameSamples;
  }
  if (!setupSamples.empty()) {
    result.setupMs = Median(setupSamples);
  }
  if (!result.throughputPerRepeat.empty()) {
    std::sort(result.throughputPerRepeat.begin(), result.throughputPerRepeat.end());
    auto middle = result.throughputPerRepeat.size() / 2;
    result.throughputMedian = result.throughputPerRepeat.size() % 2 == 0
                                  ? (result.throughputPerRepeat[middle - 1] +
                                     result.throughputPerRepeat[middle]) *
                                        0.5
                                  : result.throughputPerRepeat[middle];
  }

  result.recordMs = SampleStats::Compute(recordSamples);
  result.flushMs = SampleStats::Compute(flushSamples);
  result.submitMs = SampleStats::Compute(submitSamples);

  for (const auto& checkpoint : result.checkpoints) {
    if (checkpoint.blank) {
      result.status = "blank";
      result.statusDetail = "checkpoint frame contains no content";
      break;
    }
    if (checkpoint.hash == "readback-failed") {
      result.status = "failed";
      result.statusDetail = "checkpoint readback failed";
      break;
    }
  }

  return result;
}

}  // namespace benchmark
