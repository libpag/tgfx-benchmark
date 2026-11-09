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

#include <cstdint>
#include <string>
#include <vector>
#include "core/Scene.h"

namespace benchmark {

enum class RunMode {
  /// Legacy interactive mode: opens a window and renders with vsync. Not a data source.
  Interactive,
  /// Offscreen, pipelined submission. Primary source of comparison data.
  Measure,
  /// Offscreen, searches the largest scaling-axis value that fits the frame budget.
  Capacity,
  /// Offscreen, measures the cost of the first frame only.
  FirstFrame,
};

const char* RunModeName(RunMode mode);

struct RunConfig {
  RunMode mode = RunMode::Interactive;
  std::vector<std::string> filters = {};
  std::vector<Tier> tiers = {Tier::M};
  std::string suite = {};
  int width = 1920;
  int height = 1080;
  float density = 2.0f;
  int warmupFrames = 60;
  int measureFrames = 300;
  int repeats = 5;
  double budgetMs = 16.67;
  bool breakdownPass = true;
  bool keepSamples = true;
  bool isolate = false;
  std::string outPath = {};
  std::string checkpointDir = {};
  std::string resourcesDir = {};

  /// Internal: run exactly one scene tier. Used by the isolated mode and by external runners.
  std::string singleCase = {};
  std::string singleTier = {};

  /// Returns the frame count used by the capacity search for one probe.
  int probeFrames() const {
    return warmupFrames < 10 ? 10 : (warmupFrames > 30 ? 30 : warmupFrames);
  }
};

}  // namespace benchmark
