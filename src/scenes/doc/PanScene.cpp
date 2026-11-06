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
#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Font.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/TextLayer.h"
#include "tgfx/layers/ShapeStyle.h"

namespace benchmark {
namespace {

int64_t LayersForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1000;
    case Tier::M:
      return 10000;
    case Tier::L:
      return 100000;
    case Tier::XL:
      return 400000;
  }
  return 10000;
}

class PanScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "doc.pan";
      result.category = "doc";
      result.description =
          "A large layer tree rendered through a tiled DisplayList while the viewport pans";
      result.realWorkload = "Infinite canvas panning in a design tool";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckCache | BottleneckCPURecord;
      result.params = {{"layers", int64_t{10000}, "number of leaf layers", true},
                       {"tileSize", int64_t{256}, "tile size in pixels", false},
                       {"mode", std::string("tiled"), "tiled or direct rendering", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    params.setInt("tileSize", 256);
    params.setString("mode", "tiled");
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto layerCount = params.getInt("layers");
    auto tileSize = params.getInt("tileSize");
    auto mode = params.getString("mode");

    _displayList = std::make_unique<tgfx::DisplayList>();
    if (mode == "direct") {
      _displayList->setRenderMode(tgfx::RenderMode::Direct);
    } else {
      _displayList->setRenderMode(tgfx::RenderMode::Tiled);
      _displayList->setTileSize(static_cast<int>(tileSize));
      _displayList->setMaxTileCount(2048);
    }
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    // The document is slightly larger than the viewport and the leaves are spread uniformly over
    // it, so that raising the tier raises the on screen density instead of pushing content out of
    // view. A shallow group level keeps the tree realistic for the tiled renderer.
    Random random(0xD0C00001u);
    const auto documentWidth = _width * 1.5f;
    const auto documentHeight = _height * 1.5f;
    const auto groupCount = static_cast<int64_t>(64);
    const auto columns = static_cast<int64_t>(8);
    const auto cellWidth = documentWidth / static_cast<float>(columns);
    const auto cellHeight = documentHeight / static_cast<float>(columns);
    const auto perGroup = layerCount / groupCount + 1;

    for (int64_t groupIndex = 0; groupIndex < groupCount; groupIndex++) {
      auto group = tgfx::Layer::Make();
      auto column = groupIndex % columns;
      auto row = groupIndex / columns;
      group->setPosition(tgfx::Point{static_cast<float>(column) * cellWidth,
                                     static_cast<float>(row) * cellHeight});
      for (int64_t i = 0; i < perGroup; i++) {
        auto shape = tgfx::ShapeLayer::Make();
        tgfx::Path path;
        // Everything below is in physical pixels so that group offsets and leaf offsets share
        // the same space.
        auto size = random.range(8.0f, 48.0f) * _density;
        auto x = random.range(0.0f, cellWidth - size);
        auto y = random.range(0.0f, cellHeight - size);
        path.addRoundRect(tgfx::Rect::MakeXYWH(x, y, size, size), 4.0f * _density,
                          4.0f * _density);
        shape->setPath(path);
        shape->setFillStyle(tgfx::ShapeStyle::Make(
            tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 0.9f}));
        group->addChild(shape);
      }
      root->addChild(group);
    }
    _documentWidth = documentWidth;
    _documentHeight = documentHeight;
    return true;
  }

  void update(int64_t frameIndex) override {
    if (_displayList == nullptr) {
      return;
    }
    // A deterministic pan trajectory that sweeps across the document and back.
    auto period = 90.0;
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
  float _density = 1.0f;
  float _documentWidth = 0.0f;
  float _documentHeight = 0.0f;
  std::unique_ptr<tgfx::DisplayList> _displayList = {};
};

}  // namespace

BENCH_SCENE(PanScene);

}  // namespace benchmark
