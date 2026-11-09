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

#include <cmath>
#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"

namespace benchmark {
namespace {

int64_t RowsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 8000;
    case Tier::M:
      return 30000;
    case Tier::L:
      return 80000;
    case Tier::XL:
      return 200000;
  }
  return 30000;
}

/// A scroll container with an explicit scroll rect, which is how a virtualized list clips its
/// content. This is a different path from moving a whole display list by content offset.
class ScrollRectScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "ui.scroll_rect";
      result.category = "ui";
      result.description =
          "A tall list clipped by Layer::setScrollRect while the inner container scrolls";
      result.realWorkload = "Virtualized lists and clipped scroll containers in design tools";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckCache | BottleneckCPURecord;
      result.params = {{"rows", int64_t{2000}, "list rows", true}};
      result.checkpointFrames = {0, 45};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("rows", RowsForTier(tier));
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto rows = params.getInt("rows");

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    _rowHeight = 48.0f * _density;
    _container = tgfx::Layer::Make();
    _container->setScrollRect(tgfx::Rect::MakeWH(_width, _height));

    _inner = tgfx::Layer::Make();
    _container->addChild(_inner);

    Random random(0x5C001001u);
    for (int64_t i = 0; i < rows; i++) {
      auto row = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      path.addRect(tgfx::Rect::MakeXYWH(0.0f, static_cast<float>(i) * _rowHeight,
                                        _width - 32.0f * _density, _rowHeight - 2.0f));
      row->setPath(path);
      row->setFillStyle(tgfx::ShapeStyle::Make(
          tgfx::Color{0.35f + 0.4f * random.nextFloat(), 0.45f, 0.75f, 0.85f}));
      _inner->addChild(row);
    }
    root->addChild(_container);
    _contentHeight = static_cast<float>(rows) * _rowHeight;
    return true;
  }

  void update(int64_t frameIndex) override {
    if (_inner == nullptr) {
      return;
    }
    auto maxScroll = _contentHeight > _height ? _contentHeight - _height : 0.0f;
    auto offset = std::fmod(static_cast<float>(frameIndex) * 3.0f * _density, maxScroll + 1.0f);
    _inner->setPosition(tgfx::Point{0.0f, -offset});
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
    _inner = nullptr;
    _container = nullptr;
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
  float _rowHeight = 0.0f;
  float _contentHeight = 0.0f;
  std::unique_ptr<tgfx::DisplayList> _displayList = {};
  std::shared_ptr<tgfx::Layer> _container = {};
  std::shared_ptr<tgfx::Layer> _inner = {};
};

}  // namespace

BENCH_SCENE(ScrollRectScene);

}  // namespace benchmark
