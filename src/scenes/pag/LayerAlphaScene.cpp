/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"

namespace benchmark {
namespace {

int64_t DepthForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 2;
    case Tier::M:
      return 8;
    case Tier::L:
      return 20;
    case Tier::XL:
      return 48;
  }
  return 4;
}

/// Nested saveLayerAlpha: every level allocates an offscreen target and composites it back with
/// an alpha, which is how translucent groups and fading layers are rendered.
class LayerAlphaScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.layer_alpha";
      result.category = "pag";
      result.description = "Nested saveLayerAlpha levels with shapes drawn inside the innermost";
      result.realWorkload = "Fading layer groups and group opacity in motion graphics";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckOffscreen | BottleneckFillRate;
      result.params = {{"depth", int64_t{4}, "nested offscreen levels", true},
                       {"items", int64_t{120}, "shapes inside the innermost level", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("depth", DepthForTier(tier));
    params.setInt("items", 120);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    _depth = static_cast<int>(params.getInt("depth"));
    auto items = params.getInt("items");

    Random random(0x1A9E0001u);
    _items.clear();
    _items.reserve(static_cast<size_t>(items));
    for (int64_t i = 0; i < items; i++) {
      auto size = random.range(40.0f, 180.0f) * _density;
      _items.push_back(tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                            random.range(0.0f, _height - size), size, size));
    }
    _paint = tgfx::Paint();
    _paint.setColor(tgfx::Color{0.25f, 0.6f, 0.9f, 0.8f});
    _paint.setAntiAlias(true);
    return true;
  }

  void update(int64_t frameIndex) override {
    _angle = static_cast<float>(frameIndex % 180) * 0.5f;
  }

  void draw(tgfx::Canvas* canvas) override {
    for (auto level = 0; level < _depth; level++) {
      // Alpha compounds across levels: 0.92 over 32 levels leaves almost nothing, which turned
      // the deepest tier into a blank frame.
      canvas->saveLayerAlpha(0.98f);
      canvas->rotate(_angle * 0.02f, _width * 0.5f, _height * 0.5f);
      // Each level carries its own content, so the offscreen targets are not empty and the fill
      // work grows with the nesting depth the way a grouped layer tree does.
      for (const auto& rect : _items) {
        canvas->drawRect(rect, _paint);
      }
    }
    canvas->restoreToCount(0);
  }

  void teardown() override {
    _items.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  float _angle = 0.0f;
  int _depth = 1;
  tgfx::Paint _paint = {};
  std::vector<tgfx::Rect> _items = {};
};

}  // namespace

BENCH_SCENE(LayerAlphaScene);

}  // namespace benchmark
