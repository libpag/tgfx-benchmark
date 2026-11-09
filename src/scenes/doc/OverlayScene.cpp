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
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "scenes/common/DocumentBuilder.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"

namespace benchmark {
namespace {

int64_t SelectionsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1;
    case Tier::M:
      return 5;
    case Tier::L:
      return 20;
    case Tier::XL:
      return 100;
  }
  return 50;
}

class OverlayScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "doc.overlay";
      result.category = "doc";
      result.description =
          "Selection boxes and corner handles drawn on top of a tiled document";
      result.realWorkload = "Marquee selection and transform handles in a design tool";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckCPURecord | BottleneckCache;
      result.params = {{"selections", int64_t{50}, "number of selected layers", true},
                       {"layers", int64_t{20000}, "document leaf layers", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("selections", SelectionsForTier(tier));
    params.setInt("layers", 20000);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto selections = params.getInt("selections");
    auto layerCount = params.getInt("layers");

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Tiled);
    _displayList->setTileSize(256);
    _displayList->setMaxTileCount(4096);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    _documentWidth = _width * 1.5f;
    _documentHeight = _height * 1.5f;
    std::vector<std::shared_ptr<tgfx::Layer>> leaves;
    if (!BuildDocument(root, sceneContext, layerCount, 0x0FE10001u, _documentWidth,
                       _documentHeight, &leaves)) {
      return false;
    }
    _displayList->setContentOffset(-(_documentWidth - _width) * 0.5f,
                                   -(_documentHeight - _height) * 0.5f);

    auto outline = tgfx::ShapeStyle::Make(tgfx::Color{0.15f, 0.45f, 0.95f, 1.0f});
    Random random(0x0FE10002u);
    _boxes.clear();
    _boxes.reserve(static_cast<size_t>(selections));
    for (int64_t i = 0; i < selections; i++) {
      auto box = tgfx::ShapeLayer::Make();
      auto size = random.range(40.0f, 220.0f) * _density;
      tgfx::Path path;
      path.addRect(tgfx::Rect::MakeXYWH(0.0f, 0.0f, size, size * 0.75f));
      box->setPath(path);
      box->setStrokeStyle(outline);
      box->setLineWidth(1.5f * _density);
      auto position = tgfx::Point{random.range(0.0f, _width), random.range(0.0f, _height)};
      box->setPosition(position);
      _basePositions.push_back(position);
      // The overlay lives on top of the document and never scrolls with it.
      root->addChild(box);
      _boxes.push_back(box);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    auto phase = static_cast<float>(frameIndex % 120) / 120.0f;
    auto offset = static_cast<float>(std::sin(phase * 2.0f * static_cast<float>(M_PI))) * 8.0f;
    for (size_t i = 0; i < _boxes.size(); i++) {
      // Absolute positions: accumulating an offset would drift away from the document.
      _boxes[i]->setPosition(
          tgfx::Point{_basePositions[i].x + offset, _basePositions[i].y});
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
    _boxes.clear();
    _basePositions.clear();
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
  std::vector<std::shared_ptr<tgfx::Layer>> _boxes = {};
  std::vector<tgfx::Point> _basePositions = {};
};

}  // namespace

BENCH_SCENE(OverlayScene);

}  // namespace benchmark
