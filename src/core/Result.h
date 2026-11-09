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
#include "core/Params.h"

namespace benchmark {

struct SampleStats {
  int64_t count = 0;
  double min = 0.0;
  double median = 0.0;
  double mean = 0.0;
  double p90 = 0.0;
  double p99 = 0.0;
  double max = 0.0;
  double stddev = 0.0;
  double mad = 0.0;
  double cv = 0.0;

  static SampleStats Compute(std::vector<double> values);
};

struct CheckpointResult {
  int64_t frame = 0;
  std::string hash = {};
  bool blank = false;
  std::string fileName = {};
};

struct CapacityResult {
  std::string axis = {};
  double budgetMs = 0.0;
  std::string metric = {};
  double maxValue = 0.0;
  double lowerBound = 0.0;
  double upperBound = 0.0;
  int probes = 0;
  bool saturated = false;
};

struct CaseResult {
  std::string scene = {};
  std::string category = {};
  std::string tier = {};
  std::string paramsJSON = {};
  std::string paramHash = {};
  std::string status = "ok";
  std::string statusDetail = {};

  double setupMs = 0.0;
  std::vector<double> throughputPerRepeat = {};
  std::vector<double> throughputBlocks = {};
  double throughputMedian = 0.0;

  SampleStats frameMs = {};
  SampleStats recordMs = {};
  SampleStats flushMs = {};
  SampleStats submitMs = {};
  SampleStats gpuMs = {};
  SampleStats firstFrameMs = {};
  std::string gpuSource = "none";

  uint64_t gpuCacheBytes = 0;
  uint64_t gpuCachePeakBytes = 0;
  uint64_t footprintBytes = 0;
  uint64_t footprintPeakBytes = 0;

  CapacityResult capacity = {};

  std::vector<CheckpointResult> checkpoints = {};
  std::vector<double> samples = {};
};

}  // namespace benchmark
