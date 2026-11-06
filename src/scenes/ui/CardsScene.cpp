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
#include "tgfx/core/ImageFilter.h"

namespace benchmark {
namespace {

const char* const CardTitles[] = {
    "Quarterly report",
    "Design review",
    "Build pipeline",
    "Performance lab",
};

struct Card {
  tgfx::Rect rect = {};
  tgfx::Color accent = {};
  size_t titleIndex = 0;
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

class CardsScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "ui.cards.canvas";
      result.category = "ui";
      result.description =
          "Cards with a drop shadow, rounded corners, a title and an icon drawn through Canvas";
      result.realWorkload = "Feed and dashboard cards composited on every frame";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckOffscreen | BottleneckCPURecord;
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
    // Shadows are expensive, so the same offset is shared by every card.
    _shadowOffset = 4.0f * _density;

    auto shadowFilter = tgfx::ImageFilter::DropShadowOnly(
        0.0f, _shadowOffset, shadow, shadow, tgfx::Color{0.0f, 0.0f, 0.0f, 0.35f});
    if (shadowFilter == nullptr) {
      return false;
    }
    _shadowPaint = tgfx::Paint();
    _shadowPaint.setImageFilter(shadowFilter);

    auto typeface = sceneContext.resources->typeface("font/NotoSansSC-Regular.otf");
    if (typeface == nullptr) {
      return false;
    }
    _titleFont = tgfx::Font(typeface, 15.0f * _density);
    _titlePaint = tgfx::Paint();
    _titlePaint.setColor(tgfx::Color{0.15f, 0.16f, 0.20f, 1.0f});
    _titlePaint.setAntiAlias(true);

    _icon = sceneContext.resources->image("images/mandrill_128.png");

    const auto columns = 4;
    auto gap = 24.0f * _density;
    auto cardWidth = (_width - gap * static_cast<float>(columns + 1)) /
                     static_cast<float>(columns);
    auto cardHeight = cardWidth * 0.62f;

    Random random(0xCA4D0001u);
    _cards.clear();
    _cards.reserve(static_cast<size_t>(cards));
    for (int64_t i = 0; i < cards; i++) {
      Card card = {};
      auto column = static_cast<int>(i % columns);
      auto row = static_cast<int>(i / columns);
      auto x = gap + static_cast<float>(column) * (cardWidth + gap);
      auto y = gap + static_cast<float>(row) * (cardHeight + gap);
      card.rect = tgfx::Rect::MakeXYWH(x, y, cardWidth, cardHeight);
      card.accent = {random.nextFloat(), random.nextFloat(), random.nextFloat(), 1.0f};
      card.titleIndex = static_cast<size_t>(random.rangeInt(0, 3));
      _cards.push_back(card);
      if (y > _height) {
        break;
      }
    }
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& card : _cards) {
      auto radius = 12.0f * _density;

      // Shadow pass.
      canvas->drawRRect(tgfx::RRect::MakeRectXY(card.rect, radius, radius), _shadowPaint);

      // Card body.
      tgfx::Paint body = {};
      body.setColor(tgfx::Color{1.0f, 1.0f, 1.0f, 1.0f});
      body.setAntiAlias(true);
      canvas->drawRRect(tgfx::RRect::MakeRectXY(card.rect, radius, radius), body);

      // Accent strip and title.
      tgfx::Paint accent = {};
      accent.setColor(card.accent);
      accent.setAntiAlias(true);
      auto strip = tgfx::Rect::MakeXYWH(card.rect.left, card.rect.top, 6.0f * _density,
                                        card.rect.height());
      canvas->drawRect(strip, accent);
      canvas->drawSimpleText(CardTitles[card.titleIndex], card.rect.left + 20.0f * _density,
                             card.rect.top + 34.0f * _density, _titleFont, _titlePaint);

      if (_icon != nullptr) {
        auto iconSize = 40.0f * _density;
        auto iconRect = tgfx::Rect::MakeXYWH(card.rect.right - iconSize - 16.0f * _density,
                                             card.rect.bottom - iconSize - 16.0f * _density,
                                             iconSize, iconSize);
        canvas->drawImageRect(_icon, iconRect);
      }
    }
  }

  void teardown() override {
    _cards.clear();
    _icon = nullptr;
    _shadowPaint.setImageFilter(nullptr);
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  float _shadowOffset = 0.0f;
  tgfx::Paint _shadowPaint = {};
  tgfx::Paint _titlePaint = {};
  tgfx::Font _titleFont = {};
  std::shared_ptr<tgfx::Image> _icon = {};
  std::vector<Card> _cards = {};
};

}  // namespace

BENCH_SCENE(CardsScene);

}  // namespace benchmark
