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

#include <string>
#include "core/RunConfig.h"
#include "tgfx/gpu/Context.h"

namespace benchmark {

struct EnvironmentInfo {
  std::string tgfxCommit = {};
  std::string benchmarkCommit = {};
  std::string backend = {};
  std::string buildType = {};
  std::string os = {};
  std::string cpu = {};
  std::string gpuRenderer = {};
  std::string gpuVersion = {};
  std::string gpuVendor = {};
  int width = 0;
  int height = 0;
  float density = 1.0f;
  std::string thermalStart = {};
  std::string thermalEnd = {};
};

EnvironmentInfo CollectEnvironment(tgfx::Context* context, const RunConfig& config);

}  // namespace benchmark
