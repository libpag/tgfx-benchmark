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
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Font.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"
#include "tgfx/layers/TextLayer.h"

namespace benchmark {
namespace {

const char* const RowLabels[] = {
    "Inbox", "Drafts", "Sent", "Archive", "Projects", "Notes",
};

int64_t RowsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 50;
    case Tier::M:
      return 500;
    case Tier::L:
      return 5000;
    case Tier::XL:
      return 20000;
  }
  return 500;
}

class ListScrollScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "ui.list_scroll";
      result.category = "ui";
      result.description =
          "A long scrolling list rendered through a partially updated DisplayList";
      result.realWorkload = "Message lists and settings pages that scroll every frame";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckCache | BottleneckCPURecord;
      result.params = {{"rows", int64_t{500}, "number of list rows", true},
                       {"partial", true, "partial updates instead of full redraw", false}};
      result.checkpointFrames = {0, 45};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("rows", RowsForTier(tier));
    params.setBool("partial", true);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto rows = params.getInt("rows");
    auto partial = params.getBool("partial");

    auto typeface = sceneContext.resources->typeface("font/NotoSansSC-Regular.otf");
    if (typeface == nullptr) {
      return false;
    }
    auto font = tgfx::Font(typeface, 16.0f * _density);

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(partial ? tgfx::RenderMode::Partial : tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    _rowHeight = 56.0f * _density;
    Random random(0x11570001u);
    for (int64_t i = 0; i < rows; i++) {
      auto container = tgfx::Layer::Make();
      container->setPosition(tgfx::Point{0.0f, static_cast<float>(i) * _rowHeight});

      auto background = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      path.addRect(tgfx::Rect::MakeXYWH(0.0f, 0.0f, _width, _rowHeight - 1.0f));
      background->setPath(path);
      background->setFillStyle(tgfx::ShapeStyle::Make(
          tgfx::Color{0.97f, 0.97f, 0.98f, 1.0f}));
      container->addChild(background);

      auto label = tgfx::TextLayer::Make();
      label->setText(RowLabels[static_cast<size_t>(random.rangeInt(0, 5))]);
      label->setFont(font);
      label->setTextColor(tgfx::Color{0.16f, 0.17f, 0.21f, 1.0f});
      label->setWidth(_width - 40.0f * _density);
      label->setHeight(24.0f * _density);
      label->setPosition(tgfx::Point{20.0f * _density, 16.0f * _density});
      container->addChild(label);

      root->addChild(container);
    }
    _contentHeight = static_cast<float>(rows) * _rowHeight;
    return true;
  }

  void update(int64_t frameIndex) override {
    if (_displayList == nullptr) {
      return;
    }
    auto maxScroll = _contentHeight > _height ? _contentHeight - _height : 0.0f;
    auto speed = 4.0f * _density;
    auto offset = std::fmod(static_cast<float>(frameIndex) * speed, maxScroll + 1.0f);
    _displayList->setContentOffset(0.0f, -offset);
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
  float _rowHeight = 0.0f;
  float _contentHeight = 0.0f;
  std::unique_ptr<tgfx::DisplayList> _displayList = {};
};

}  // namespace

BENCH_SCENE(ListScrollScene);

}  // namespace benchmark
