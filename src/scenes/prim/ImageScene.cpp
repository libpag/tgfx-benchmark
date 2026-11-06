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

#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/SamplingOptions.h"

namespace benchmark {
namespace {

struct ImageQuad {
  tgfx::Rect rect = {};
  size_t samplingIndex = 0;
};

int64_t CountForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 100;
    case Tier::M:
      return 1000;
    case Tier::L:
      return 5000;
    case Tier::XL:
      return 20000;
  }
  return 1000;
}

class ImageScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.image";
      result.category = "prim";
      result.description = "Scaled image draws alternating nearest, linear and mipmapped sampling";
      result.realWorkload = "Cover art, thumbnails and video frames composited into a scene";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckBandwidth | BottleneckUpload;
      result.params = {{"count", int64_t{1000}, "number of image draws", true},
                       {"minSize", 24.0, "smallest destination edge in logical pixels", false},
                       {"maxSize", 320.0, "largest destination edge in logical pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("count", CountForTier(tier));
    params.setDouble("minSize", 24.0);
    params.setDouble("maxSize", 320.0);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto count = params.getInt("count");
    auto minSize = static_cast<float>(params.getDouble("minSize")) * _density;
    auto maxSize = static_cast<float>(params.getDouble("maxSize")) * _density;

    _image = sceneContext.resources->image("images/mandrill_128.png");
    if (_image == nullptr) {
      return false;
    }

    _samplings[0] = tgfx::SamplingOptions(tgfx::FilterMode::Nearest, tgfx::MipmapMode::None);
    _samplings[1] = tgfx::SamplingOptions(tgfx::FilterMode::Linear, tgfx::MipmapMode::None);
    _samplings[2] = tgfx::SamplingOptions(tgfx::FilterMode::Linear, tgfx::MipmapMode::Linear);

    Random random(0x1A6E0000u);
    _quads.clear();
    _quads.reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; i++) {
      ImageQuad quad = {};
      auto size = random.range(minSize, maxSize);
      quad.rect = tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                       random.range(0.0f, _height - size), size, size);
      quad.samplingIndex = static_cast<size_t>(i) % 3;
      _quads.push_back(quad);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    auto drift = static_cast<float>(frameIndex % 60) * 0.5f * _density;
    for (auto& quad : _quads) {
      quad.rect.offset(drift * 0.01f, 0.0f);
    }
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& quad : _quads) {
      canvas->drawImageRect(_image, quad.rect, _samplings[quad.samplingIndex]);
    }
  }

  void teardown() override {
    _quads.clear();
    _image = nullptr;
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::shared_ptr<tgfx::Image> _image = {};
  tgfx::SamplingOptions _samplings[3] = {};
  std::vector<ImageQuad> _quads = {};
};

}  // namespace

BENCH_SCENE(ImageScene);

}  // namespace benchmark
