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

#include <cmath>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Path.h"

namespace benchmark {
namespace {

struct StrokeItem {
  tgfx::Path path = {};
  tgfx::Paint paint = {};
};

int64_t CountForTier(Tier tier) {
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

class StrokeScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.stroke";
      result.category = "prim";
      result.description =
          "Polylines and quadratic curves stroked with mixed widths, caps and joins";
      result.realWorkload = "Chart axes, map routes and icon outlines";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckVertex | BottleneckCPURecord;
      result.params = {{"count", int64_t{1000}, "number of stroked paths", true},
                       {"points", int64_t{6}, "points per path", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("count", CountForTier(tier));
    params.setInt("points", 6);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto count = params.getInt("count");
    auto points = params.getInt("points");

    Random random(0x57A0E001u);
    _items.clear();
    _items.reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; i++) {
      StrokeItem item = {};
      auto x = random.range(0.0f, _width);
      auto y = random.range(0.0f, _height);
      auto step = random.range(12.0f, 40.0f) * _density;
      item.path.moveTo(x, y);
      for (int64_t p = 1; p < points; p++) {
        auto nextX = x + static_cast<float>(p) * step * random.range(-1.0f, 1.0f);
        auto nextY = y + step * random.range(-1.0f, 1.0f);
        if (p % 2 == 0) {
          item.path.quadTo(x + step * 0.5f, y - step, nextX, nextY);
        } else {
          item.path.lineTo(nextX, nextY);
        }
      }
      auto width = (i % 3 == 0) ? 1.0f : ((i % 3 == 1) ? 2.5f : 8.0f);
      item.paint = tgfx::Paint();
      item.paint.setStyle(tgfx::PaintStyle::Stroke);
      item.paint.setStrokeWidth(width * _density);
      item.paint.setLineCap((i % 2 == 0) ? tgfx::LineCap::Round : tgfx::LineCap::Butt);
      item.paint.setLineJoin((i % 2 == 0) ? tgfx::LineJoin::Round : tgfx::LineJoin::Bevel);
      item.paint.setColor(tgfx::Color{0.9f, 0.35f, 0.2f, 0.85f});
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
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::vector<StrokeItem> _items = {};
};

}  // namespace

BENCH_SCENE(StrokeScene);

}  // namespace benchmark
