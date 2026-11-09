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
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/ImageFilter.h"

namespace benchmark {
namespace {

struct ShadowShape {
  tgfx::RRect rrect = {};
  tgfx::Paint paint = {};
};

int64_t LayersForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 4;
    case Tier::M:
      return 16;
    case Tier::L:
      return 32;
    case Tier::XL:
      return 96;
  }
  return 16;
}

class ShadowScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.shadow";
      result.category = "pag";
      result.description =
          "Rounded shapes rendered with drop shadow and inner shadow image filters";
      result.realWorkload = "Cards, buttons and panels with shadows in motion graphics";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckOffscreen | BottleneckFillRate;
      result.params = {{"layers", int64_t{16}, "number of shadowed shapes", true},
                       {"radius", 12.0, "shadow sigma in logical pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    params.setDouble("radius", 12.0);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto layers = params.getInt("layers");
    auto radius = static_cast<float>(params.getDouble("radius")) * _density;

    auto dropShadow = tgfx::ImageFilter::DropShadowOnly(
        0.0f, 4.0f * _density, radius, radius, tgfx::Color{0.0f, 0.0f, 0.0f, 0.45f});
    auto innerShadow = tgfx::ImageFilter::InnerShadowOnly(
        0.0f, 2.0f * _density, radius * 0.5f, radius * 0.5f,
        tgfx::Color{0.0f, 0.0f, 0.0f, 0.5f});
    if (dropShadow == nullptr || innerShadow == nullptr) {
      return false;
    }

    Random random(0x5A0D0001u);
    _shapes.clear();
    _shapes.reserve(static_cast<size_t>(layers));
    for (int64_t i = 0; i < layers; i++) {
      ShadowShape shape = {};
      auto size = random.range(160.0f, 420.0f) * _density;
      auto rect = tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                       random.range(0.0f, _height - size), size, size * 0.6f);
      shape.rrect = tgfx::RRect::MakeRectXY(rect, 16.0f * _density, 16.0f * _density);
      shape.paint = tgfx::Paint();
      shape.paint.setImageFilter((i % 2 == 0) ? dropShadow : innerShadow);
      _shapes.push_back(shape);
    }
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& shape : _shapes) {
      canvas->drawRRect(shape.rrect, shape.paint);
    }
  }

  void teardown() override {
    _shapes.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::vector<ShadowShape> _shapes = {};
};

}  // namespace

BENCH_SCENE(ShadowScene);

}  // namespace benchmark
