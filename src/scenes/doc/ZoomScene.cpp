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
      return 500;
    case Tier::M:
      return 5000;
    case Tier::L:
      return 20000;
    case Tier::XL:
      return 80000;
  }
  return 10000;
}

class ZoomScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "doc.zoom";
      result.category = "doc";
      result.description =
          "A tiled DisplayList zoomed from 0.1x to 8x and back, forcing continuous tile "
          "rasterization";
      result.realWorkload = "Zooming an infinite canvas, where every zoom step re-rasterizes "
                            "vector content";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckCache | BottleneckCPURecord;
      result.params = {{"layers", int64_t{10000}, "number of leaf layers", true},
                       {"updateMode", std::string("immediate"),
                        "immediate or smooth tile refinement", false},
                       {"maxTilesPerFrame", int64_t{8},
                        "tiles refined per frame in smooth mode", false}};
      result.checkpointFrames = {20, 45};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    params.setString("updateMode", "immediate");
    params.setInt("maxTilesPerFrame", 8);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    auto layerCount = params.getInt("layers");
    auto updateMode = params.getString("updateMode");
    auto maxTilesPerFrame = params.getInt("maxTilesPerFrame");

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Tiled);
    _displayList->setTileSize(256);
    _displayList->setMaxTileCount(4096);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    if (updateMode == "smooth") {
      _displayList->setTileUpdateMode(tgfx::TileUpdateMode::Smooth);
      _displayList->setMaxTilesRefinedPerFrame(static_cast<int>(maxTilesPerFrame));
    } else {
      _displayList->setTileUpdateMode(tgfx::TileUpdateMode::Immediate);
    }

    auto root = _displayList->root();
    _documentWidth = _width * 1.5f;
    _documentHeight = _height * 1.5f;
    return BuildDocument(root, sceneContext, layerCount, 0x20010001u, _documentWidth,
                         _documentHeight);
  }

  void update(int64_t frameIndex) override {
    if (_displayList == nullptr) {
      return;
    }
    // Triangle wave in log space so that every zoom level receives the same amount of frames.
    const auto period = 120.0;
    auto phase = static_cast<double>(frameIndex % static_cast<int64_t>(period)) / period;
    auto triangle = phase < 0.5 ? phase * 2.0 : 2.0 - phase * 2.0;
    const auto minZoom = std::log(0.1);
    const auto maxZoom = std::log(8.0);
    auto zoom = static_cast<float>(std::exp(minZoom + triangle * (maxZoom - minZoom)));
    _displayList->setZoomScale(zoom);
    auto offsetX = -(_documentWidth * zoom - _width) * 0.5f;
    auto offsetY = -(_documentHeight * zoom - _height) * 0.5f;
    _displayList->setContentOffset(offsetX, offsetY);
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

BENCH_SCENE(ZoomScene);

}  // namespace benchmark
