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
#include "core/SceneRegistry.h"
#include "tgfx/core/ImageFilter.h"

namespace benchmark {
namespace {

struct BlurLayer {
  std::vector<tgfx::Rect> rects = {};
  tgfx::Paint paint = {};
};

int64_t LayersForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1;
    case Tier::M:
      return 2;
    case Tier::L:
      return 4;
    case Tier::XL:
      return 8;
  }
  return 4;
}

class BlurScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.blur";
      result.category = "pag";
      result.description =
          "Content rendered into saveLayer with a blur image filter, one layer per group";
      result.realWorkload = "Gaussian blur effects and frosted panels in motion graphics";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckOffscreen | BottleneckFillRate;
      result.params = {{"layers", int64_t{4}, "number of blurred layers", true},
                       {"radius", 16.0, "gaussian sigma in logical pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    params.setDouble("radius", 16.0);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto layers = params.getInt("layers");
    auto radius = static_cast<float>(params.getDouble("radius")) * _density;

    auto filter = tgfx::ImageFilter::Blur(radius, radius, tgfx::TileMode::Decal);
    if (filter == nullptr) {
      return false;
    }

    Random random(0x1B1A0001u);
    _layers.clear();
    _layers.reserve(static_cast<size_t>(layers));
    for (int64_t i = 0; i < layers; i++) {
      BlurLayer layer = {};
      layer.paint = tgfx::Paint();
      layer.paint.setImageFilter(filter);
      auto count = random.rangeInt(4, 12);
      for (auto j = 0; j < count; j++) {
        auto size = random.range(120.0f, 360.0f) * _density;
        layer.rects.push_back(tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                                   random.range(0.0f, _height - size), size, size));
      }
      _layers.push_back(layer);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    auto drift = static_cast<float>(frameIndex % 90) * 0.4f * _density;
    for (auto& layer : _layers) {
      for (size_t i = 0; i < layer.rects.size(); i++) {
        auto& rect = layer.rects[i];
        rect.offsetTo(rect.left + drift * 0.02f, rect.top);
      }
    }
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& layer : _layers) {
      canvas->saveLayer(&layer.paint);
      auto index = 0;
      for (const auto& rect : layer.rects) {
        tgfx::Paint paint = {};
        paint.setColor(tgfx::Color{0.2f + 0.15f * static_cast<float>(index % 4),
                                   0.6f - 0.1f * static_cast<float>(index % 4), 0.9f, 1.0f});
        canvas->drawRect(rect, paint);
        index++;
      }
      canvas->restore();
    }
  }

  void teardown() override {
    _layers.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::vector<BlurLayer> _layers = {};
};

}  // namespace

BENCH_SCENE(BlurScene);

}  // namespace benchmark
