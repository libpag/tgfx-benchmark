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
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>
#include "core/CaseResultIO.h"
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

  // A device is still created here so the report carries the GPU identity.
  auto device = CreateHeadlessDevice();
  auto context = device == nullptr ? nullptr : device->lockContext();

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
    std::cerr << "capacity mode is not implemented yet\n";
    return 2;
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
