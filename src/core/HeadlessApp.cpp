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
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "core/HeadlessApp.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>
#include "core/CaseResultIO.h"
#include "core/Checkpoint.h"
#include "core/FrameTimer.h"
#include "core/HeadlessDevice.h"
#include "core/MeasureDriver.h"
#include "core/Process.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "core/report/Environment.h"
#include "core/report/ReportWriter.h"
#include "core/report/SystemInfo.h"
#include "tgfx/platform/Print.h"

namespace benchmark {
namespace {

using CaseId = std::pair<std::string, Tier>;

void ApplyParamOverrides(ParamSet* params, const std::vector<std::string>& overrides) {
  for (const auto& item : overrides) {
    auto position = item.find('=');
    if (position == std::string::npos) {
      continue;
    }
    auto name = item.substr(0, position);
    auto value = item.substr(position + 1);
    if (value == "true" || value == "false") {
      params->setBool(name, value == "true");
      continue;
    }
    char* end = nullptr;
    auto integer = std::strtoll(value.c_str(), &end, 10);
    if (end != nullptr && *end == '\0' && !value.empty()) {
      params->setInt(name, integer);
      continue;
    }
    auto real = std::strtod(value.c_str(), &end);
    if (end != nullptr && *end == '\0' && !value.empty()) {
      params->setDouble(name, real);
      continue;
    }
    params->setString(name, value);
  }
}

/// Returns the parameter the capacity search varies, or an empty string when the scene has none.
std::string ScalingAxis(const SceneInfo& info) {
  for (const auto& param : info.params) {
    if (param.isScalingAxis) {
      return param.name;
    }
  }
  return {};
}

Tier ParseTierOrDefault(const std::string& text) {
  Tier tier = Tier::M;
  ParseTier(text, &tier);
  return tier;
}

void PrintHeader() {
  std::printf("%-24s %-4s %10s %10s %10s %12s  %s\n", "scene", "tier", "median_ms", "p90_ms",
              "gpu_ms", "count", "status");
}

void PrintRow(const CaseResult& result) {
  std::printf("%-24s %-4s %10.3f %10.3f %10.3f %12lld  %s\n", result.scene.c_str(),
              result.tier.c_str(), result.throughputMedian, result.frameMs.p90,
              result.gpuMs.median, static_cast<long long>(result.frameMs.count),
              result.status.c_str());
  std::fflush(stdout);
}

CaseResult MeasureFirstFrame(Scene* scene, Tier tier, const ParamSet& params,
                             const SceneContext& sceneContext, const RunConfig& config,
                             const std::string& checkpointDir) {
  CaseResult result = {};
  result.scene = scene->info().id;
  result.category = scene->info().category;
  result.tier = TierName(tier);
  result.paramsJSON = params.toCanonicalJSON();
  result.paramHash = params.hash();

  auto context = sceneContext.context;
  auto surface = tgfx::Surface::Make(context, config.width, config.height);
  if (surface == nullptr) {
    result.status = "failed";
    result.statusDetail = "failed to create offscreen surface";
    return result;
  }

  auto setupStart = FrameTimer::NowMs();
  if (!scene->setup(sceneContext, params)) {
    result.status = "failed";
    result.statusDetail = "scene setup failed";
    return result;
  }
  result.setupMs = FrameTimer::NowMs() - setupStart;

  // A cold first frame includes shader compilation, pipeline creation and the first upload, so the
  // GPU is waited on instead of being allowed to lag behind.
  auto frameStart = FrameTimer::NowMs();
  scene->update(0);
  surface->getCanvas()->clear(BenchmarkBackgroundColor());
  scene->render(surface.get());
  auto recording = context->flush();
  if (recording != nullptr) {
    context->submit(std::move(recording), true);
  }
  auto frameEnd = FrameTimer::NowMs();
  result.firstFrameMs = SampleStats::Compute({frameEnd - frameStart});

  auto checkpoint = Checkpoint::Capture(surface.get(), checkpointDir,
                                        result.scene + "-" + result.tier + "-first", 0,
                                        BenchmarkBackgroundColor());
  result.checkpoints.push_back(checkpoint);
  if (checkpoint.blank) {
    result.status = "blank";
    result.statusDetail = "first frame contains no content";
  }
  scene->teardown();
  return result;
}

void PrintFirstFrameRow(const CaseResult& result) {
  std::printf("%-24s %-4s %10.3f %10.3f %10.3f %12lld  %s\n", result.scene.c_str(),
              result.tier.c_str(), result.firstFrameMs.median, result.firstFrameMs.p90,
              result.setupMs, static_cast<long long>(result.firstFrameMs.count),
              result.status.c_str());
  std::fflush(stdout);
}

std::vector<CaseId> ResolveCases(const RunConfig& config) {
  std::vector<CaseId> cases;
  if (!config.singleCase.empty()) {
    Tier tier = Tier::M;
    ParseTier(config.singleTier, &tier);
    cases.emplace_back(config.singleCase, tier);
    return cases;
  }
  for (const auto& id : SceneRegistry::Resolve(config.filters)) {
    for (auto tier : config.tiers) {
      cases.emplace_back(id, tier);
    }
  }
  return cases;
}

std::string TempResultPath(const std::string& outPath, int index) {
  auto directory = outPath.empty() ? std::filesystem::temp_directory_path()
                                   : std::filesystem::path(outPath).parent_path();
  std::ostringstream name;
  name << "bench-case-" << index << ".tmp";
  return (directory / name.str()).string();
}

std::string Number(double value) {
  std::ostringstream stream;
  stream.precision(10);
  stream << value;
  return stream.str();
}

int RunFirstFrameIsolated(const CommandLineOptions& options) {
  const auto& config = options.config;
  auto executable = ExecutablePath();
  if (executable.empty()) {
    std::cerr << "isolated mode needs the executable path, which is unavailable here\n";
    return 3;
  }
  std::vector<CaseResult> results;
  std::printf("%-24s %-4s %10s %10s %10s %12s  %s\n", "scene", "tier", "cold_ms", "p90_ms",
              "setup_ms", "samples", "status");
  auto index = 0;
  for (const auto& item : ResolveCases(config)) {
    CaseResult combined = {};
    combined.scene = item.first;
    combined.tier = TierName(item.second);
    combined.category = "";
    std::vector<double> samples;
    for (auto repeat = 0; repeat < config.repeats; repeat++) {
      auto tempPath = TempResultPath(config.outPath, index++);
      std::vector<std::string> arguments = {
          executable,
          "--mode=first-frame",
          "--single=" + item.first + ":" + TierName(item.second),
          "--out=" + tempPath,
          "--resolution=" + std::to_string(config.width) + "x" + std::to_string(config.height),
          "--density=" + Number(static_cast<double>(config.density)),
      };
      if (!config.checkpointDir.empty() && repeat == 0) {
        arguments.push_back("--checkpoint-dir=" + config.checkpointDir);
      }
      if (!config.resourcesDir.empty()) {
        arguments.push_back("--resources=" + config.resourcesDir);
      }
      for (const auto& param : config.paramOverrides) {
        arguments.push_back("--param=" + param);
      }
      auto exitCode = RunProcess(arguments);
      CaseResult child = {};
      if (exitCode >= 0 && CaseResultIO::Read(tempPath, &child)) {
        samples.push_back(child.firstFrameMs.median);
        combined.setupMs = std::max(combined.setupMs, child.setupMs);
        combined.category = child.category;
        combined.paramsJSON = child.paramsJSON;
        combined.paramHash = child.paramHash;
        combined.status = child.status;
        combined.statusDetail = child.statusDetail;
        if (repeat == 0) {
          combined.checkpoints = child.checkpoints;
        }
      } else {
        combined.status = "failed";
        combined.statusDetail = "isolated child exited with " + std::to_string(exitCode);
        break;
      }
      std::error_code error = {};
      std::filesystem::remove(tempPath, error);
    }
    combined.firstFrameMs = SampleStats::Compute(samples);
    PrintFirstFrameRow(combined);
    results.push_back(std::move(combined));
  }

  // The device is created only now: keeping one alive while the children run slows their first
  // frame down by an order of magnitude on Metal.
  auto device = CreateHeadlessDevice();
  auto context = device == nullptr ? nullptr : device->lockContext();
  auto environment = CollectEnvironment(context, config);
  environment.thermalEnd = ThermalStateName();
  auto json =
      ReportWriter::BuildResult(environment, config, ReportWriter::CurrentRunId(), results);
  if (device != nullptr) {
    device->unlock();
  }
  if (config.outPath.empty()) {
    std::printf("\n%s\n", json.c_str());
  } else if (!ReportWriter::Write(config.outPath, json)) {
    return 4;
  }
  for (const auto& result : results) {
    if (result.status == "failed") {
      return 1;
    }
  }
  return 0;
}

int RunCapacity(const CommandLineOptions& options) {
  const auto& config = options.config;
  auto executable = ExecutablePath();
  if (executable.empty()) {
    std::cerr << "capacity mode needs the executable path, which is unavailable here\n";
    return 3;
  }

  const auto budget = config.budgetMs;
  const auto frames = std::min(std::max(config.measureFrames, 30), 60);
  const auto warmup = std::min(std::max(config.warmupFrames, 5), 15);

  std::vector<CaseResult> results;
  std::printf("%-24s %-4s %-10s %12s %8s %10s  %s\n", "scene", "tier", "axis", "max_value",
              "probes", "budget_ms", "status");
  auto index = 0;

  for (const auto& item : ResolveCases(config)) {
    auto scene = SceneRegistry::Create(item.first);
    if (scene == nullptr) {
      continue;
    }
    const auto& info = scene->info();
    CaseResult result = {};
    result.scene = info.id;
    result.category = info.category;
    result.tier = TierName(item.second);

    auto axis = ScalingAxis(info);
    if (axis.empty()) {
      result.status = "skipped";
      result.statusDetail = "scene has no scaling axis";
      std::printf("%-24s %-4s %-10s %12s %8d %10.2f  %s\n", info.id.c_str(),
                  result.tier.c_str(), "-", "-", 0, budget, result.status.c_str());
      results.push_back(std::move(result));
      continue;
    }

    auto baseParams = scene->paramsForTier(item.second);
    ApplyParamOverrides(&baseParams, config.paramOverrides);
    result.paramsJSON = baseParams.toCanonicalJSON();
    result.paramHash = baseParams.hash();
    auto startValue = baseParams.getDouble(axis);
    if (startValue <= 0.0) {
      startValue = 1.0;
    }

    auto probes = 0;
    auto probe = [&](double value) -> double {
      probes++;
      auto tempPath = TempResultPath(config.outPath, index++);
      std::vector<std::string> arguments = {
          executable,
          "--mode=measure",
          "--single=" + info.id + ":" + result.tier,
          "--out=" + tempPath,
          "--warmup=" + std::to_string(warmup),
          "--frames=" + std::to_string(frames),
          "--repeat=1",
          "--no-breakdown",
          "--no-samples",
          "--resolution=" + std::to_string(config.width) + "x" + std::to_string(config.height),
          "--density=" + Number(static_cast<double>(config.density)),
      };
      if (!config.resourcesDir.empty()) {
        arguments.push_back("--resources=" + config.resourcesDir);
      }
      for (const auto& param : config.paramOverrides) {
        if (param.rfind(axis + "=", 0) == 0) {
          continue;
        }
        arguments.push_back("--param=" + param);
      }
      arguments.push_back("--param=" + axis + "=" + Number(value));

      auto exitCode = RunProcess(arguments);
      CaseResult child = {};
      auto ok = exitCode >= 0 && CaseResultIO::Read(tempPath, &child);
      std::error_code error = {};
      std::filesystem::remove(tempPath, error);
      if (!ok || child.status != "ok") {
        return -1.0;
      }
      // The sustained cost per frame, not the median of individual frames: when the GPU is the
      // bottleneck the CPU races ahead and single frame times understate the load.
      return child.throughputMedian > 0.0 ? child.throughputMedian : child.frameMs.p90;
    };

    auto startLatency = probe(startValue);
    if (startLatency < 0.0) {
      result.status = "failed";
      result.statusDetail = "probe failed at the tier default";
      results.push_back(std::move(result));
      continue;
    }

    double low = 0.0;
    double high = 0.0;
    if (startLatency > budget) {
      // Even the tier default is over budget: search downwards.
      high = startValue;
      low = startValue * 0.5;
      while (low > 1.0 && probes < 8) {
        auto latency = probe(low);
        if (latency < 0.0) {
          low = 0.0;
          break;
        }
        if (latency <= budget) {
          break;
        }
        high = low;
        low *= 0.5;
      }
    } else {
      low = startValue;
      high = startValue * 2.0;
      auto previous = startLatency;
      auto saturated = false;
      while (probes < 8 && high < 1e8) {
        auto latency = probe(high);
        if (latency < 0.0 || latency > budget) {
          break;
        }
        // Doubling the load must at least nudge the frame time. Otherwise the scene has hit a
        // ceiling (content clipped, culled or saturated) and a larger value means nothing.
        if (latency < previous * 1.02) {
          saturated = true;
          break;
        }
        previous = latency;
        low = high;
        high *= 2.0;
      }
      if (saturated) {
        high = low * 2.0;
        result.capacity.saturated = true;
      }
    }

    if (low <= 0.0 || high <= low) {
      result.status = "failed";
      result.statusDetail = "capacity search did not bracket the budget";
      results.push_back(std::move(result));
      continue;
    }

    while (high / low > 1.05 && probes < 12) {
      auto middle = std::sqrt(low * high);
      auto latency = probe(middle);
      if (latency < 0.0) {
        break;
      }
      if (latency <= budget) {
        low = middle;
      } else {
        high = middle;
      }
    }

    result.status = "ok";
    result.capacity.axis = axis;
    result.capacity.budgetMs = budget;
    result.capacity.metric = "throughput_ms";
    result.capacity.maxValue = low;
    result.capacity.lowerBound = low;
    result.capacity.upperBound = high;
    result.capacity.probes = probes;
    std::printf("%-24s %-4s %-10s %12.0f %8d %10.2f  %s\n", info.id.c_str(),
                result.tier.c_str(), axis.c_str(), low, probes, budget, "ok");
    std::fflush(stdout);
    results.push_back(std::move(result));
  }

  auto device = CreateHeadlessDevice();
  auto context = device == nullptr ? nullptr : device->lockContext();
  auto environment = CollectEnvironment(context, config);
  environment.thermalEnd = ThermalStateName();
  auto json =
      ReportWriter::BuildResult(environment, config, ReportWriter::CurrentRunId(), results);
  if (device != nullptr) {
    device->unlock();
  }
  if (config.outPath.empty()) {
    std::printf("\n%s\n", json.c_str());
  } else if (!ReportWriter::Write(config.outPath, json)) {
    return 4;
  }
  for (const auto& result : results) {
    if (result.status == "failed") {
      return 1;
    }
  }
  return 0;
}

/// Runs every case in its own process. The GPU caches of one scene never get a chance to inflate
/// the memory readings of the next one, and a crash only loses a single case.
int RunIsolated(const CommandLineOptions& options, const char* fallbackResourcesDir) {
  const auto& config = options.config;
  (void)fallbackResourcesDir;
  auto executable = ExecutablePath();
  if (executable.empty()) {
    std::cerr << "isolated mode needs the executable path, which is unavailable here\n";
    return 3;
  }

  std::vector<CaseResult> results;
  PrintHeader();
  auto index = 0;
  for (const auto& item : ResolveCases(config)) {
    auto tempPath = TempResultPath(config.outPath, index++);
    std::vector<std::string> arguments = {
        executable,
        "--mode=measure",
        "--single=" + item.first + ":" + TierName(item.second),
        "--out=" + tempPath,
        "--warmup=" + std::to_string(config.warmupFrames),
        "--frames=" + std::to_string(config.measureFrames),
        "--repeat=" + std::to_string(config.repeats),
        "--resolution=" + std::to_string(config.width) + "x" + std::to_string(config.height),
        "--density=" + Number(static_cast<double>(config.density)),
        config.breakdownPass ? "--breakdown" : "--no-breakdown",
        config.keepSamples ? "--samples" : "--no-samples",
    };
    if (!config.checkpointDir.empty()) {
      arguments.push_back("--checkpoint-dir=" + config.checkpointDir);
    }
    if (!config.resourcesDir.empty()) {
      arguments.push_back("--resources=" + config.resourcesDir);
    }
    for (const auto& param : config.paramOverrides) {
      arguments.push_back("--param=" + param);
    }

    auto exitCode = RunProcess(arguments);
    CaseResult result = {};
    if (exitCode >= 0 && CaseResultIO::Read(tempPath, &result)) {
      PrintRow(result);
      results.push_back(std::move(result));
    } else {
      CaseResult failed = {};
      failed.scene = item.first;
      failed.tier = TierName(item.second);
      failed.status = "failed";
      failed.statusDetail = "isolated child exited with " + std::to_string(exitCode);
      PrintRow(failed);
      results.push_back(std::move(failed));
    }
    std::error_code error = {};
    std::filesystem::remove(tempPath, error);
  }

  // Created after the children finish so it never competes with them for the GPU.
  auto device = CreateHeadlessDevice();
  auto context = device == nullptr ? nullptr : device->lockContext();
  auto environment = CollectEnvironment(context, config);
  environment.thermalEnd = ThermalStateName();
  auto json =
      ReportWriter::BuildResult(environment, config, ReportWriter::CurrentRunId(), results);
  if (device != nullptr) {
    device->unlock();
  }

  if (config.outPath.empty()) {
    std::printf("\n%s\n", json.c_str());
  } else if (!ReportWriter::Write(config.outPath, json)) {
    return 4;
  }
  for (const auto& result : results) {
    if (result.status != "ok") {
      return 1;
    }
  }
  return 0;
}

}  // namespace

void PrintSceneList() {
  std::printf("%-24s %-8s %-12s %s\n", "scene", "tier", "category", "description");
  for (const auto& id : SceneRegistry::Names()) {
    auto scene = SceneRegistry::Create(id);
    if (scene == nullptr) {
      continue;
    }
    const auto& info = scene->info();
    std::printf("%-24s %-8s %-12s %s\n", info.id.c_str(), "all", info.category.c_str(),
                info.description.c_str());
    for (auto tier : {Tier::S, Tier::M, Tier::L, Tier::XL}) {
      auto params = scene->paramsForTier(tier);
      std::printf("%-24s %-8s %s\n", "", TierName(tier), params.toCanonicalJSON().c_str());
    }
  }
}

int RunHeadless(const CommandLineOptions& options, const char* fallbackResourcesDir) {
  const auto& config = options.config;
  if (config.mode == RunMode::Capacity) {
    return RunCapacity(options);
  }
  if (config.mode == RunMode::FirstFrame && config.singleCase.empty()) {
    return RunFirstFrameIsolated(options);
  }
  if (config.isolate && config.singleCase.empty()) {
    return RunIsolated(options, fallbackResourcesDir);
  }

  auto device = CreateHeadlessDevice();
  if (device == nullptr) {
    std::cerr << "failed to create a headless device for the configured backend\n";
    return 3;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    std::cerr << "failed to lock the benchmark context\n";
    return 3;
  }

  auto resourcesDir =
      config.resourcesDir.empty() ? std::string(fallbackResourcesDir) : config.resourcesDir;
  ResourceStore resources(std::move(resourcesDir));

  SceneContext sceneContext = {};
  sceneContext.context = context;
  sceneContext.resources = &resources;
  sceneContext.width = config.width;
  sceneContext.height = config.height;
  sceneContext.density = config.density;

  const auto single = !config.singleCase.empty();

  if (config.mode == RunMode::FirstFrame) {
    // Only reachable with --single; the parent drives one child per repeat.
    auto scene = SceneRegistry::Create(config.singleCase);
    if (scene == nullptr) {
      device->unlock();
      return 1;
    }
    auto params = scene->paramsForTier(ParseTierOrDefault(config.singleTier));
    ApplyParamOverrides(&params, config.paramOverrides);
    auto result = MeasureFirstFrame(scene.get(), ParseTierOrDefault(config.singleTier), params,
                                    sceneContext, config, config.checkpointDir);
    device->unlock();
    return CaseResultIO::Write(config.outPath, result) ? 0 : 4;
  }
  std::vector<CaseResult> results;
  if (!single) {
    PrintHeader();
  }
  for (const auto& item : ResolveCases(config)) {
    auto scene = SceneRegistry::Create(item.first);
    if (scene == nullptr) {
      continue;
    }
    auto params = scene->paramsForTier(item.second);
    ApplyParamOverrides(&params, config.paramOverrides);
    auto result = MeasureDriver::Run(scene.get(), item.second, params, sceneContext, config,
                                     config.checkpointDir);
    if (!single) {
      PrintRow(result);
    }
    results.push_back(std::move(result));
  }

  if (single) {
    device->unlock();
    if (results.empty() || config.outPath.empty()) {
      return 1;
    }
    return CaseResultIO::Write(config.outPath, results.front()) ? 0 : 4;
  }

  auto environment = CollectEnvironment(context, config);
  environment.thermalEnd = ThermalStateName();
  auto json =
      ReportWriter::BuildResult(environment, config, ReportWriter::CurrentRunId(), results);
  device->unlock();

  if (config.outPath.empty()) {
    std::printf("\n%s\n", json.c_str());
  } else if (!ReportWriter::Write(config.outPath, json)) {
    return 4;
  }

  for (const auto& result : results) {
    if (result.status != "ok") {
      return 1;
    }
  }
  return 0;
}

}  // namespace benchmark
