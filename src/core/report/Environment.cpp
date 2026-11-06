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

#include "core/report/Environment.h"
#include "core/report/SystemInfo.h"

#ifndef BENCH_TGFX_COMMIT
#define BENCH_TGFX_COMMIT "unknown"
#endif

#ifndef BENCH_BENCHMARK_COMMIT
#define BENCH_BENCHMARK_COMMIT "unknown"
#endif

#ifndef BENCH_BACKEND_NAME
#define BENCH_BACKEND_NAME "unknown"
#endif

#ifndef BENCH_BUILD_TYPE
#define BENCH_BUILD_TYPE "unknown"
#endif

namespace benchmark {

EnvironmentInfo CollectEnvironment(tgfx::Context* context, const RunConfig& config) {
  EnvironmentInfo info = {};
  info.tgfxCommit = BENCH_TGFX_COMMIT;
  info.benchmarkCommit = BENCH_BENCHMARK_COMMIT;
  info.backend = BENCH_BACKEND_NAME;
  info.buildType = BENCH_BUILD_TYPE;
  info.os = OperatingSystemName();
  info.cpu = CpuName();
  info.width = config.width;
  info.height = config.height;
  info.density = config.density;
  info.thermalStart = ThermalStateName();
  info.thermalEnd = info.thermalStart;
  if (context != nullptr && context->gpu() != nullptr) {
    auto gpuInfo = context->gpu()->info();
    if (gpuInfo != nullptr) {
      info.gpuRenderer = gpuInfo->renderer;
      info.gpuVersion = gpuInfo->version;
      info.gpuVendor = gpuInfo->vendor;
    }
  }
  return info;
}

}  // namespace benchmark
