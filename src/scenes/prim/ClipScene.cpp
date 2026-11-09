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

#include <algorithm>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Path.h"

namespace benchmark {
namespace {

int64_t DepthForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1;
    case Tier::M:
      return 4;
    case Tier::L:
      return 16;
    case Tier::XL:
      return 64;
  }
  return 4;
}

class ClipScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.clip";
      result.category = "prim";
      result.description =
          "Nested rect, round rect and path clips with shape draws inside the deepest level";
      result.realWorkload = "Scrolling lists, image masks and nested containers in UI";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCPURecord | BottleneckOffscreen;
      result.params = {{"depth", int64_t{4}, "nesting depth of the clip stack", true},
                       {"items", int64_t{200}, "draws inside the innermost clip", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("depth", DepthForTier(tier));
    params.setInt("items", 200);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    _depth = params.getInt("depth");
    auto items = params.getInt("items");

    Random random(0xC11F0001u);
    _items.clear();
    _items.reserve(static_cast<size_t>(items));
    for (int64_t i = 0; i < items; i++) {
      auto size = random.range(16.0f, 64.0f) * _density;
      _items.push_back(tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                            random.range(0.0f, _height - size), size, size));
    }
    _paint = tgfx::Paint();
    _paint.setColor(tgfx::Color{0.15f, 0.45f, 0.75f, 0.8f});
    _paint.setAntiAlias(true);

    _path = tgfx::Path();
    auto diameter = std::min(_width, _height) * 0.98f;
    _path.addOval(tgfx::Rect::MakeXYWH((_width - diameter) * 0.5f,
                                       (_height - diameter) * 0.5f, diameter, diameter));
    return true;
  }

  void update(int64_t frameIndex) override {
    _rotation = static_cast<float>(frameIndex % 360);
  }

  void draw(tgfx::Canvas* canvas) override {
    canvas->save();
    for (int64_t level = 0; level < _depth; level++) {
      canvas->save();
      auto inset = static_cast<float>(level) * 2.0f * _density;
      auto rect = tgfx::Rect::MakeXYWH(inset, inset, _width - inset * 2.0f,
                                       _height - inset * 2.0f);
      switch (level % 3) {
        case 0:
          canvas->clipRect(rect, true);
          break;
        case 1: {
          auto radius = 24.0f * _density;
          canvas->clipRRect(tgfx::RRect::MakeRectXY(rect, radius, radius), true);
          break;
        }
        default: {
          canvas->save();
          canvas->rotate(_rotation, rect.centerX(), rect.centerY());
          canvas->clipPath(_path, true);
          break;
        }
      }
    }

    for (const auto& rect : _items) {
      canvas->drawRect(rect, _paint);
    }

    // Everything pushed above is popped in one go.
    canvas->restoreToCount(0);
  }

  void teardown() override {
    _items.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  int64_t _depth = 0;
  float _rotation = 0.0f;
  tgfx::Paint _paint = {};
  tgfx::Path _path = {};
  std::vector<tgfx::Rect> _items = {};
};

}  // namespace

BENCH_SCENE(ClipScene);

}  // namespace benchmark
