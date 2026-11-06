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
#include "tgfx/core/MaskFilter.h"
#include "tgfx/core/Shader.h"

namespace benchmark {
namespace {

int64_t GroupsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 2;
    case Tier::M:
      return 8;
    case Tier::L:
      return 32;
    case Tier::XL:
      return 128;
  }
  return 8;
}

class MatteScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.matte";
      result.category = "pag";
      result.description =
          "Image content drawn through an alpha mask driven by a radial gradient shader";
      result.realWorkload = "Track mattes, gradient fades and luma keys in motion graphics";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckOffscreen | BottleneckFillRate;
      result.params = {{"groups", int64_t{8}, "number of masked groups", true},
                       {"inverted", false, "inverts the alpha mask", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("groups", GroupsForTier(tier));
    params.setBool("inverted", false);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto groups = params.getInt("groups");
    auto inverted = params.getBool("inverted");

    _image = sceneContext.resources->image("images/mandrill_128.png");
    if (_image == nullptr) {
      return false;
    }

    Random random(0x3A770001u);
    _groups.clear();
    _groups.reserve(static_cast<size_t>(groups));
    for (int64_t i = 0; i < groups; i++) {
      MatteGroup group = {};
      auto size = random.range(320.0f, 720.0f) * _density;
      group.rect = tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                        random.range(0.0f, _height - size), size, size);
      auto colors = std::vector<tgfx::Color>{
          {1.0f, 1.0f, 1.0f, 1.0f},
          {1.0f, 1.0f, 1.0f, 0.35f},
          {1.0f, 1.0f, 1.0f, 0.0f},
      };
      auto shader = tgfx::Shader::MakeRadialGradient(
          {group.rect.centerX(), group.rect.centerY()}, size * 0.5f, colors);
      if (shader == nullptr) {
        return false;
      }
      group.paint = tgfx::Paint();
      group.paint.setMaskFilter(tgfx::MaskFilter::MakeShader(shader, inverted));
      _groups.push_back(group);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    auto drift = static_cast<float>(frameIndex % 120) * 0.6f * _density;
    for (auto& group : _groups) {
      group.rect.offsetTo(group.rect.left + drift * 0.01f, group.rect.top);
    }
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& group : _groups) {
      canvas->drawImageRect(_image, group.rect, tgfx::SamplingOptions(), &group.paint);
    }
  }

  void teardown() override {
    _groups.clear();
    _image = nullptr;
  }

 private:
  struct MatteGroup {
    tgfx::Rect rect = {};
    tgfx::Paint paint = {};
  };

  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::shared_ptr<tgfx::Image> _image = {};
  std::vector<MatteGroup> _groups = {};
};

}  // namespace

BENCH_SCENE(MatteScene);

}  // namespace benchmark
