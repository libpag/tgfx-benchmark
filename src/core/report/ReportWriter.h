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
#include <vector>
#include "core/Result.h"
#include "core/RunConfig.h"
#include "core/report/Environment.h"

namespace benchmark {

class ReportWriter {
 public:
  static std::string BuildResult(const EnvironmentInfo& environment, const RunConfig& config,
                                 const std::string& runId,
                                 const std::vector<CaseResult>& results);

  static std::string BuildSpec();

  static bool Write(const std::string& path, const std::string& text);

  static std::string CurrentRunId();
};

}  // namespace benchmark
