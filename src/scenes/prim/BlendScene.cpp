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

#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"

namespace benchmark {
namespace {

struct BackdropTile {
  tgfx::Rect rect = {};
  tgfx::Color color = {};
};

const tgfx::BlendMode AdvancedModes[] = {
    tgfx::BlendMode::Multiply,  tgfx::BlendMode::Screen,    tgfx::BlendMode::Overlay,
    tgfx::BlendMode::Darken,    tgfx::BlendMode::Lighten,   tgfx::BlendMode::ColorDodge,
    tgfx::BlendMode::Difference, tgfx::BlendMode::HardLight,
};

constexpr size_t AdvancedModeCount = sizeof(AdvancedModes) / sizeof(AdvancedModes[0]);

int64_t PassesForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 4;
    case Tier::M:
      return 16;
    case Tier::L:
      return 64;
    case Tier::XL:
      return 256;
  }
  return 16;
}

class BlendScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.blend";
      result.category = "prim";
      result.description = "Full screen translucent rects composited with advanced blend modes";
      result.realWorkload = "Layer compositing with multiply, screen, overlay and friends";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckFillRate | BottleneckBandwidth;
      result.params = {{"passes", int64_t{16}, "number of full screen blend passes", true}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("passes", PassesForTier(tier));
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    auto passes = params.getInt("passes");

    _paints.clear();
    _paints.reserve(static_cast<size_t>(passes));
    for (int64_t i = 0; i < passes; i++) {
      tgfx::Paint paint = {};
      auto index = static_cast<size_t>(i) % AdvancedModeCount;
      paint.setColor(tgfx::Color{0.2f + 0.1f * static_cast<float>(index),
                                 0.9f - 0.08f * static_cast<float>(index), 0.5f, 0.55f});
      paint.setBlendMode(AdvancedModes[index]);
      _paints.push_back(paint);
    }

    // A backdrop is needed for the blend passes to produce visible structure instead of one flat
    // color, which would make the checkpoint useless.
    Random random(0xB1E4D001u);
    _backdrop.clear();
    _backdrop.reserve(64);
    for (auto i = 0; i < 64; i++) {
      BackdropTile tile = {};
      auto size = random.range(120.0f, 480.0f) * sceneContext.density;
      tile.rect = tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                       random.range(0.0f, _height - size), size, size);
      tile.color = {random.nextFloat(), random.nextFloat(), random.nextFloat(), 1.0f};
      _backdrop.push_back(tile);
    }
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& tile : _backdrop) {
      tgfx::Paint paint = {};
      paint.setColor(tile.color);
      canvas->drawRect(tile.rect, paint);
    }
    auto rect = tgfx::Rect::MakeWH(_width, _height);
    for (const auto& paint : _paints) {
      canvas->drawRect(rect, paint);
    }
  }

  void teardown() override {
    _paints.clear();
    _backdrop.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  std::vector<tgfx::Paint> _paints = {};
  std::vector<BackdropTile> _backdrop = {};
};

}  // namespace

BENCH_SCENE(BlendScene);

}  // namespace benchmark
