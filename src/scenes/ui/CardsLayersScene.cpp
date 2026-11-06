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
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Font.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/ImageLayer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/TextLayer.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "tgfx/layers/ShapeStyle.h"

namespace benchmark {
namespace {

const char* const LayerCardTitles[] = {
    "Quarterly report",
    "Design review",
    "Build pipeline",
    "Performance lab",
};

int64_t CardsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 20;
    case Tier::M:
      return 100;
    case Tier::L:
      return 500;
    case Tier::XL:
      return 2000;
  }
  return 100;
}

class CardsLayersScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "ui.cards.layers";
      result.category = "ui";
      result.description =
          "The same cards built from ShapeLayer, TextLayer and ImageLayer with a drop shadow style";
      result.realWorkload = "Layer based UI where every element is a retained layer";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckOffscreen | BottleneckCache;
      result.params = {{"cards", int64_t{100}, "number of cards", true},
                       {"shadow", 8.0, "shadow blur radius in logical pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("cards", CardsForTier(tier));
    params.setDouble("shadow", 8.0);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto cards = params.getInt("cards");
    auto shadow = static_cast<float>(params.getDouble("shadow")) * _density;

    auto typeface = sceneContext.resources->typeface("font/NotoSansSC-Regular.otf");
    if (typeface == nullptr) {
      return false;
    }
    auto font = tgfx::Font(typeface, 15.0f * _density);
    auto icon = sceneContext.resources->image("images/mandrill_128.png");

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    auto shadowStyle = tgfx::DropShadowStyle::Make(
        0.0f, 4.0f * _density, shadow, shadow, tgfx::Color{0.0f, 0.0f, 0.0f, 0.35f}, true);
    auto white = tgfx::ShapeStyle::Make(tgfx::Color{1.0f, 1.0f, 1.0f, 1.0f});

    const auto columns = 4;
    auto gap = 24.0f * _density;
    auto cardWidth = (_width - gap * static_cast<float>(columns + 1)) /
                     static_cast<float>(columns);
    auto cardHeight = cardWidth * 0.62f;
    auto radius = 12.0f * _density;

    Random random(0xC45D0001u);
    for (int64_t i = 0; i < cards; i++) {
      auto column = static_cast<int>(i % columns);
      auto row = static_cast<int>(i / columns);
      auto x = gap + static_cast<float>(column) * (cardWidth + gap);
      auto y = gap + static_cast<float>(row) * (cardHeight + gap);
      if (y > _height) {
        break;
      }

      auto container = tgfx::Layer::Make();
      container->setPosition(tgfx::Point{x, y});

      auto body = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      path.addRoundRect(tgfx::Rect::MakeWH(cardWidth, cardHeight), radius, radius);
      body->setPath(path);
      body->setFillStyle(white);
      body->setLayerStyles({shadowStyle});
      container->addChild(body);

      auto title = tgfx::TextLayer::Make();
      title->setText(LayerCardTitles[random.rangeInt(0, 3)]);
      title->setFont(font);
      title->setTextColor(tgfx::Color{0.15f, 0.16f, 0.20f, 1.0f});
      title->setWidth(cardWidth - 32.0f * _density);
      title->setHeight(28.0f * _density);
      title->setPosition(tgfx::Point{20.0f * _density, 22.0f * _density});
      container->addChild(title);

      if (icon != nullptr) {
        auto image = tgfx::ImageLayer::Make();
        image->setImage(icon);
        image->setPosition(tgfx::Point{cardWidth - 56.0f * _density,
                                       cardHeight - 56.0f * _density});
        container->addChild(image);
      }

      root->addChild(container);
      _layers.push_back(container);
    }
    return true;
  }

  void update(int64_t) override {
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

BENCH_SCENE(CardsLayersScene);

}  // namespace benchmark
