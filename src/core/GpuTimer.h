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
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <vector>

namespace benchmark {

/**
 * Measures the time the GPU spends on the work submitted between Begin() and End().
 *
 * Implementations must not stall the pipeline: results are read back only once the driver reports
 * them ready, and Collect(force = true) is the only blocking call. Returns nullptr when the active
 * backend cannot provide GPU timestamps, in which case the driver falls back to a synchronised
 * submit and reports that upper bound instead.
 */
class GpuTimer {
 public:
  virtual ~GpuTimer() = default;

  /// Identifier written to the report, for example "gl_timer_query".
  virtual const char* source() const = 0;

  virtual void Begin() = 0;

  virtual void End() = 0;

  /// Appends every sample that is ready. With force set it waits for all outstanding samples.
  virtual void Collect(std::vector<double>* samples, bool force) = 0;
};

/// Created per case; the GL context must be current when this is called.
std::unique_ptr<GpuTimer> CreateGpuTimer();

}  // namespace benchmark
