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

#include "core/HeadlessApp.h"
#include <cstdio>
#include <iostream>
#include <vector>
#include "core/HeadlessDevice.h"
#include "core/MeasureDriver.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "core/report/Environment.h"
#include "core/report/ReportWriter.h"
#include "core/report/SystemInfo.h"
#include "tgfx/platform/Print.h"

namespace benchmark {
namespace {

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

  auto device = CreateHeadlessDevice();
  if (device == nullptr) {
    std::cerr << "failed to create a headless device for the configured backend\n";
    return 3;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    std::cerr << "failed to lock the benchmart context\n";
    return 3;
  }

  auto resourcesDir = config.resourcesDir.empty() ? std::string(fallbackResourcesDir)
                                                  : config.resourcesDir;
  ResourceStore resources(std::move(resourcesDir));

  SceneContext sceneContext = {};
  sceneContext.context = context;
  sceneContext.resources = &resources;
  sceneContext.width = config.width;
  sceneContext.height = config.height;
  sceneContext.density = config.density;

  std::vector<CaseResult> results;
  PrintHeader();
  for (const auto& id : SceneRegistry::Resolve(config.filters)) {
    for (auto tier : config.tiers) {
      auto scene = SceneRegistry::Create(id);
      if (scene == nullptr) {
        continue;
      }
      auto params = scene->paramsForTier(tier);
      auto result = MeasureDriver::Run(scene.get(), tier, params, sceneContext, config,
                                       config.checkpointDir);
      PrintRow(result);
      results.push_back(std::move(result));
    }
  }

  auto environment = CollectEnvironment(context, config);
  environment.thermalEnd = ThermalStateName();
  auto json = ReportWriter::BuildResult(environment, config, ReportWriter::CurrentRunId(),
                                        results);
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
