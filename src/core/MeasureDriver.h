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
#include "core/Result.h"
#include "core/RunConfig.h"
#include "core/Scene.h"

namespace benchmark {

class MeasureDriver {
 public:
  /**
   * Runs one scene at one tier. The driver owns the offscreen surface and the frame loop; the
   * scene only produces content. Timings are taken around the three submission stages so the
   * result can be attributed to recording, flushing and GPU work.
   */
  static CaseResult Run(Scene* scene, Tier tier, const ParamSet& params,
                        const SceneContext& sceneContext, const RunConfig& config,
                        const std::string& checkpointDir);
};

}  // namespace benchmark
