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

#include <cmath>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/PathEffect.h"

namespace benchmark {
namespace {

struct DashedPath {
  tgfx::Path path = {};
  tgfx::Paint paint = {};
};

int64_t PathsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 100;
    case Tier::M:
      return 1000;
    case Tier::L:
      return 4000;
    case Tier::XL:
      return 16000;
  }
  return 1000;
}

class StrokeHeavyScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "vec.stroke_heavy";
      result.category = "vec";
      result.description =
          "Dense dashed and solid outlines with thick strokes and mixed joins";
      result.realWorkload = "Map routes, charts and technical drawings";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCPURecord | BottleneckVertex;
      result.params = {{"paths", int64_t{1000}, "number of stroked paths", true},
                       {"dashed", true, "applies a dash path effect", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("paths", PathsForTier(tier));
    params.setBool("dashed", true);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto count = params.getInt("paths");
    auto dashed = params.getBool("dashed");

    _dash = nullptr;
    if (dashed) {
      const float intervals[2] = {12.0f * _density, 6.0f * _density};
      _dash = tgfx::PathEffect::MakeDash(intervals, 2, 0.0f);
      if (_dash == nullptr) {
        return false;
      }
    }

    Random random(0x5D45A001u);
    _items.clear();
    _items.reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; i++) {
      DashedPath item = {};
      auto x = random.range(0.0f, _width);
      auto y = random.range(0.0f, _height);
      auto radius = random.range(20.0f, 120.0f) * _density;
      auto steps = random.rangeInt(8, 24);
      for (auto step = 0; step <= steps; step++) {
        auto angle = static_cast<float>(step) / static_cast<float>(steps) * 2.0f *
                     static_cast<float>(M_PI);
        auto px = x + std::cos(angle) * radius;
        auto py = y + std::sin(angle) * radius;
        if (step == 0) {
          item.path.moveTo(px, py);
        } else {
          item.path.lineTo(px, py);
        }
      }
      if (_dash != nullptr && !_dash->filterPath(&item.path)) {
        return false;
      }
      item.paint = tgfx::Paint();
      item.paint.setStyle(tgfx::PaintStyle::Stroke);
      item.paint.setStrokeWidth(random.range(1.5f, 6.0f) * _density);
      item.paint.setLineJoin((i % 2 == 0) ? tgfx::LineJoin::Miter : tgfx::LineJoin::Round);
      item.paint.setColor(tgfx::Color{0.2f, 0.55f, 0.85f, 0.9f});
      item.paint.setAntiAlias(true);
      _items.push_back(item);
    }
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& item : _items) {
      canvas->drawPath(item.path, item.paint);
    }
  }

  void teardown() override {
    _items.clear();
    _dash = nullptr;
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::shared_ptr<tgfx::PathEffect> _dash = {};
  std::vector<DashedPath> _items = {};
};

}  // namespace

BENCH_SCENE(StrokeHeavyScene);

}  // namespace benchmark
