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

#include "core/Result.h"
#include <algorithm>
#include <cmath>

namespace benchmark {
namespace {

double Percentile(const std::vector<double>& sorted, double fraction) {
  if (sorted.empty()) {
    return 0.0;
  }
  auto count = static_cast<double>(sorted.size());
  auto rank = std::ceil(fraction * count) - 1.0;
  if (rank < 0.0) {
    rank = 0.0;
  }
  auto maxIndex = static_cast<double>(sorted.size() - 1);
  if (rank > maxIndex) {
    rank = maxIndex;
  }
  auto index = static_cast<size_t>(rank);
  return sorted[index];
}

}  // namespace

SampleStats SampleStats::Compute(std::vector<double> values) {
  SampleStats stats = {};
  stats.count = static_cast<int64_t>(values.size());
  if (values.empty()) {
    return stats;
  }
  std::sort(values.begin(), values.end());
  stats.min = values.front();
  stats.max = values.back();
  stats.median = Percentile(values, 0.5);
  stats.p90 = Percentile(values, 0.90);
  stats.p99 = Percentile(values, 0.99);

  double total = 0.0;
  for (auto value : values) {
    total += value;
  }
  stats.mean = total / static_cast<double>(values.size());

  double squaredTotal = 0.0;
  for (auto value : values) {
    auto delta = value - stats.mean;
    squaredTotal += delta * delta;
  }
  stats.stddev = std::sqrt(squaredTotal / static_cast<double>(values.size()));
  stats.cv = stats.mean > 0.0 ? stats.stddev / stats.mean : 0.0;

  std::vector<double> deviations;
  deviations.reserve(values.size());
  for (auto value : values) {
    deviations.push_back(std::abs(value - stats.median));
  }
  std::sort(deviations.begin(), deviations.end());
  stats.mad = Percentile(deviations, 0.5);
  return stats;
}

}  // namespace benchmark
