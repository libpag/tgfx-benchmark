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
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Context.h"

namespace benchmark {

class ResourceStore;

enum class Tier { S, M, L, XL };

const char* TierName(Tier tier);

bool ParseTier(const std::string& text, Tier* tier);

enum ModuleFlag : uint32_t {
  ModuleCore = 1u << 0,
  ModuleLayers = 1u << 1,
  ModuleSVG = 1u << 2,
};

enum BottleneckFlag : uint32_t {
  BottleneckCPURecord = 1u << 0,
  BottleneckVertex = 1u << 1,
  BottleneckFillRate = 1u << 2,
  BottleneckBandwidth = 1u << 3,
  BottleneckOffscreen = 1u << 4,
  BottleneckUpload = 1u << 5,
  BottleneckCache = 1u << 6,
  BottleneckShaderCompile = 1u << 7,
};

/// Everything a scene needs from the host. The size is in physical pixels; density maps logical
/// units to physical pixels so that content is comparable across displays.
struct SceneContext {
  tgfx::Context* context = nullptr;
  ResourceStore* resources = nullptr;
  int width = 1920;
  int height = 1080;
  float density = 2.0f;
};

struct SceneInfo {
  std::string id;
  std::string category;
  std::string description;
  std::string realWorkload;
  uint32_t modules = ModuleCore;
  uint32_t bottlenecks = BottleneckCPURecord;
  std::vector<ParamSpec> params = {};
  std::vector<int64_t> checkpointFrames = {0, 59};
};

/**
 * A benchmark scene. Scenes never read the wall clock or the mouse: all animation is driven by the
 * frame index so that runs are reproducible. Content is built in setup(), which is excluded from
 * the measured frame time.
 */
class Scene {
 public:
  virtual ~Scene() = default;

  virtual const SceneInfo& info() const = 0;

  virtual ParamSet paramsForTier(Tier tier) const = 0;

  virtual bool setup(const SceneContext& sceneContext, const ParamSet& params) = 0;

  virtual void update(int64_t frameIndex) = 0;

  /// Draws with the Canvas API. Scenes that render through a DisplayList override render() instead.
  virtual void draw(tgfx::Canvas* canvas) = 0;

  virtual void render(tgfx::Surface* surface) {
    draw(surface->getCanvas());
  }

  virtual void teardown() {
  }
};

}  // namespace benchmark
