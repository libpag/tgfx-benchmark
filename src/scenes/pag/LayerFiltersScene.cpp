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
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cmath>
#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/ColorMatrixFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"

namespace benchmark {
namespace {

int64_t LayersForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 4;
    case Tier::M:
      return 24;
    case Tier::L:
      return 96;
    case Tier::XL:
      return 384;
  }
  return 24;
}

/// Layer level filters are a separate pipeline from Canvas image filters: the layer renders into
/// an offscreen surface and the filter chain runs on the layer's output. libpag puts blur, drop
/// shadow and colour matrices here, so measuring the Canvas path does not cover it.
class LayerFiltersScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.layer_filters";
      result.category = "pag";
      result.description =
          "Layers carrying a blur filter, a drop shadow filter or a blur plus colour matrix chain";
      result.realWorkload = "Layer effects in libpag compositions and design tool effects panels";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckOffscreen | BottleneckFillRate;
      result.params = {{"layers", int64_t{24}, "filtered layers", true}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto layers = params.getInt("layers");

    const std::array<float, 20> matrix = {
        0.9f, 0.1f, 0.0f, 0.0f, 0.05f, 0.0f, 0.85f, 0.15f, 0.0f, 0.0f,
        0.05f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    };
    auto blur = tgfx::BlurFilter::Make(8.0f * _density, 8.0f * _density);
    auto shadow = tgfx::DropShadowFilter::Make(
        0.0f, 6.0f * _density, 12.0f * _density, 12.0f * _density,
        tgfx::Color{0.0f, 0.0f, 0.0f, 0.4f}, false);
    auto smallBlur = tgfx::BlurFilter::Make(4.0f * _density, 4.0f * _density);
    auto colorMatrix = tgfx::ColorMatrixFilter::Make(matrix);
    if (blur == nullptr || shadow == nullptr || smallBlur == nullptr ||
        colorMatrix == nullptr) {
      return false;
    }

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    Random random(0xF1170001u);
    auto columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(layers))));
    if (columns < 1) {
      columns = 1;
    }
    auto rows = static_cast<int>((layers + columns - 1) / columns);
    auto cell = std::min(_width / static_cast<float>(columns),
                         _height / static_cast<float>(rows));

    _layers.clear();
    _layers.reserve(static_cast<size_t>(layers));
    for (int64_t i = 0; i < layers; i++) {
      auto layer = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      auto size = cell * 0.7f;
      path.addRoundRect(tgfx::Rect::MakeWH(size, size), 12.0f * _density, 12.0f * _density);
      layer->setPath(path);
      layer->setFillStyle(tgfx::ShapeStyle::Make(
          tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 1.0f}));
      layer->setPosition(tgfx::Point{cell * static_cast<float>(i % columns) + cell * 0.15f,
                                     cell * static_cast<float>(i / columns) + cell * 0.15f});
      switch (i % 3) {
        case 0:
          layer->setFilters({blur});
          break;
        case 1:
          layer->setFilters({shadow});
          break;
        default:
          // A chain: the second filter reads the output of the first.
          layer->setFilters({smallBlur, colorMatrix});
          break;
      }
      root->addChild(layer);
      _layers.push_back(layer);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    // Re-compositing every frame: the filter chain has to run again even though the geometry is
    // static, which is exactly the cost this scene is here to measure.
    auto phase = static_cast<float>(frameIndex % 120) / 120.0f;
    for (size_t i = 0; i < _layers.size(); i++) {
      auto offset = std::sin(phase * 6.2831853f + static_cast<float>(i) * 0.5f) * 4.0f;
      _layers[i]->setAlpha(0.85f + offset * 0.01f);
    }
  }

  void draw(tgfx::Canvas* canvas) override {
    (void)canvas;
  }

  void render(tgfx::Surface* surface) override {
    if (_displayList != nullptr) {
      _displayList->render(surface, true);
    }
  }

  void teardown() override {
    _layers.clear();
    if (_displayList != nullptr) {
      auto root = _displayList->root();
      if (root != nullptr) {
        root->setChildren({});
      }
      _displayList = nullptr;
    }
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::unique_ptr<tgfx::DisplayList> _displayList = {};
  std::vector<std::shared_ptr<tgfx::Layer>> _layers = {};
};

}  // namespace

BENCH_SCENE(LayerFiltersScene);

}  // namespace benchmark
