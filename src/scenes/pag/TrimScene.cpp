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

#include <cmath>
#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/PathEffect.h"

namespace benchmark {
namespace {

struct TrimItem {
  tgfx::Path base = {};
  tgfx::Paint paint = {};
};

int64_t PathsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 20;
    case Tier::M:
      return 100;
    case Tier::L:
      return 250;
    case Tier::XL:
      return 600;
  }
  return 100;
}

/// Trim start and end animation: the path geometry is rebuilt with the effect on every frame,
/// which is what a PAG trim path layer costs.
class TrimScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.trim";
      result.category = "pag";
      result.description =
          "Stroked paths trimmed to a changing start and end segment on every frame";
      result.realWorkload = "Line drawing, progress rings and trim path animation in libpag";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCPURecord | BottleneckVertex;
      result.params = {{"paths", int64_t{250}, "number of trimmed paths", true},
                       {"segments", int64_t{48}, "points per path", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("paths", PathsForTier(tier));
    params.setInt("segments", 48);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto paths = params.getInt("paths");
    auto segments = params.getInt("segments");

    Random random(0x7B1A0001u);
    _items.clear();
    _items.reserve(static_cast<size_t>(paths));
    for (int64_t i = 0; i < paths; i++) {
      TrimItem item = {};
      auto centerX = random.range(0.0f, _width);
      auto centerY = random.range(0.0f, _height);
      auto radius = random.range(30.0f, 140.0f) * _density;
      for (int64_t s = 0; s <= segments; s++) {
        auto angle = static_cast<float>(s) / static_cast<float>(segments) * 6.2831853f;
        auto x = centerX + std::cos(angle) * radius;
        auto y = centerY + std::sin(angle) * radius * 0.7f;
        if (s == 0) {
          item.base.moveTo(x, y);
        } else {
          item.base.lineTo(x, y);
        }
      }
      item.paint = tgfx::Paint();
      item.paint.setStyle(tgfx::PaintStyle::Stroke);
      item.paint.setStrokeWidth(3.0f * _density);
      item.paint.setLineCap(tgfx::LineCap::Round);
      item.paint.setColor(tgfx::Color{0.95f, 0.45f, 0.15f, 1.0f});
      item.paint.setAntiAlias(true);
      _items.push_back(item);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    // A shrinking head and growing tail, the usual trim animation.
    auto phase = static_cast<float>(frameIndex % 120) / 120.0f;
    _start = phase * 0.5f;
    _end = _start + 0.5f;
  }

  void draw(tgfx::Canvas* canvas) override {
    auto effect = tgfx::PathEffect::MakeTrim(_start, _end);
    if (effect == nullptr) {
      return;
    }
    for (const auto& item : _items) {
      auto trimmed = item.base;
      if (effect->filterPath(&trimmed)) {
        canvas->drawPath(trimmed, item.paint);
      }
    }
  }

  void teardown() override {
    _items.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  float _start = 0.0f;
  float _end = 1.0f;
  std::vector<TrimItem> _items = {};
};

}  // namespace

BENCH_SCENE(TrimScene);

}  // namespace benchmark
