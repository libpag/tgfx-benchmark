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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <memory>
#include <string>
#include "core/SceneRegistry.h"
#include "scenes/common/DocumentBuilder.h"
#include "tgfx/layers/DisplayList.h"

namespace benchmark {
namespace {

int64_t LayersForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1000;
    case Tier::M:
      return 10000;
    case Tier::L:
      return 50000;
    case Tier::XL:
      return 200000;
  }
  return 10000;
}

class ModeCompareScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "doc.mode_compare";
      result.category = "doc";
      result.description =
          "The same document and pan trajectory rendered through Direct, Partial or Tiled mode";
      result.realWorkload = "Choosing a render mode for a design canvas at a given document size";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckCache | BottleneckCPURecord;
      result.params = {{"layers", int64_t{10000}, "number of leaf layers", true},
                       {"mode", std::string("tiled"), "direct, partial or tiled", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    params.setString("mode", "tiled");
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    auto layerCount = params.getInt("layers");
    auto mode = params.getString("mode");

    _displayList = std::make_unique<tgfx::DisplayList>();
    if (mode == "direct") {
      _displayList->setRenderMode(tgfx::RenderMode::Direct);
    } else if (mode == "partial") {
      _displayList->setRenderMode(tgfx::RenderMode::Partial);
    } else {
      _displayList->setRenderMode(tgfx::RenderMode::Tiled);
      _displayList->setTileSize(256);
      _displayList->setMaxTileCount(4096);
    }
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});

    _documentWidth = _width * 1.5f;
    _documentHeight = _height * 1.5f;
    return BuildDocument(_displayList->root(), sceneContext, layerCount, 0x30DE0001u,
                         _documentWidth, _documentHeight);
  }

  void update(int64_t frameIndex) override {
    if (_displayList == nullptr) {
      return;
    }
    const auto period = 90.0;
    auto phase = static_cast<double>(frameIndex % static_cast<int64_t>(period)) / period;
    auto angle = phase * 2.0 * M_PI;
    auto offsetX = static_cast<float>((std::sin(angle) * 0.5 + 0.5) * (_documentWidth - _width));
    auto offsetY =
        static_cast<float>((std::cos(angle) * 0.5 + 0.5) * (_documentHeight - _height));
    _displayList->setContentOffset(-offsetX, -offsetY);
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
  float _documentWidth = 0.0f;
  float _documentHeight = 0.0f;
  std::unique_ptr<tgfx::DisplayList> _displayList = {};
};

}  // namespace

BENCH_SCENE(ModeCompareScene);

}  // namespace benchmark
