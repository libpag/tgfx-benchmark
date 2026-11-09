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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <string>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Shape.h"
#include "tgfx/core/TextBlob.h"

namespace benchmark {
namespace {

int64_t CardsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 24;
    case Tier::M:
      return 360;
    case Tier::L:
      return 1000;
    case Tier::XL:
      return 3000;
  }
  return 24;
}

struct Card {
  tgfx::Rect body = {};
  tgfx::Rect thumb = {};
  tgfx::Rect pill = {};
  tgfx::Point dot = {};
  tgfx::Point lineFrom = {};
  tgfx::Point lineTo = {};
  tgfx::Point textPosition = {};
  size_t titleIndex = 0;
};

/// A dashboard built the way a real UI is: rounded cards, circular status dots, pill badges,
/// dividers, thumbnails, text blobs and vector icons, all in one pass.
class DashboardScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "ui.dashboard";
      result.category = "ui";
      result.description =
          "Cards composed of round rects, circles, ovals, lines, thumbnails, text blobs and shapes";
      result.realWorkload = "A metrics dashboard or activity feed rendered every frame";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCPURecord | BottleneckFillRate;
      result.params = {{"cards", int64_t{24}, "dashboard cards", true}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("cards", CardsForTier(tier));
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto cards = params.getInt("cards");

    _image = sceneContext.resources->image("images/mandrill_128.png");
    if (_image == nullptr) {
      return false;
    }
    auto typeface = sceneContext.resources->typeface("font/NotoSansSC-Regular.otf");
    if (typeface == nullptr) {
      return false;
    }
    auto font = tgfx::Font(typeface, 16.0f * _density);
    _titles.clear();
    for (const auto* text : {"Revenue", "Active users", "Latency", "Errors"}) {
      auto blob = tgfx::TextBlob::MakeFrom(text, font);
      if (blob == nullptr) {
        return false;
      }
      _titles.push_back(blob);
    }

    // A small vector icon reused by every card.
    tgfx::Path iconPath;
    auto iconSize = 18.0f * _density;
    iconPath.moveTo(0.0f, iconSize * 0.5f);
    iconPath.lineTo(iconSize * 0.4f, 0.0f);
    iconPath.lineTo(iconSize, iconSize * 0.5f);
    iconPath.lineTo(iconSize * 0.4f, iconSize);
    iconPath.close();
    _icon = tgfx::Shape::MakeFrom(iconPath);
    if (_icon == nullptr) {
      return false;
    }

    _cardPaint = tgfx::Paint();
    _cardPaint.setColor(tgfx::Color{1.0f, 1.0f, 1.0f, 1.0f});
    _cardPaint.setAntiAlias(true);

    _accentPaint = tgfx::Paint();
    _accentPaint.setColor(tgfx::Color{0.2f, 0.55f, 0.95f, 1.0f});
    _accentPaint.setAntiAlias(true);

    _linePaint = tgfx::Paint();
    _linePaint.setColor(tgfx::Color{0.88f, 0.89f, 0.92f, 1.0f});
    _linePaint.setStrokeWidth(1.0f * _density);
    _linePaint.setAntiAlias(true);

    _textPaint = tgfx::Paint();
    _textPaint.setColor(tgfx::Color{0.14f, 0.15f, 0.19f, 1.0f});
    _textPaint.setAntiAlias(true);

    _iconPaint = tgfx::Paint();
    _iconPaint.setColor(tgfx::Color{0.35f, 0.72f, 0.45f, 1.0f});
    _iconPaint.setAntiAlias(true);

    Random random(0xDA5B0001u);
    const auto columns = 3;
    auto gap = 20.0f * _density;
    auto cardWidth = (_width - gap * static_cast<float>(columns + 1)) /
                     static_cast<float>(columns);
    auto cardHeight = cardWidth * 0.42f;
    _cards.clear();
    _cards.reserve(static_cast<size_t>(cards));
    for (int64_t i = 0; i < cards; i++) {
      Card card = {};
      auto x = gap + static_cast<float>(i % columns) * (cardWidth + gap);
      auto y = gap + static_cast<float>(i / columns) * (cardHeight + gap);
      card.body = tgfx::Rect::MakeXYWH(x, y, cardWidth, cardHeight);
      auto thumbSize = cardHeight * 0.5f;
      card.thumb = tgfx::Rect::MakeXYWH(x + gap, y + gap, thumbSize, thumbSize);
      card.dot = tgfx::Point{x + cardWidth - gap * 1.5f, y + gap * 1.2f};
      card.pill = tgfx::Rect::MakeXYWH(x + cardWidth - gap * 4.0f, y + cardHeight - gap * 1.8f,
                                       gap * 3.0f, gap * 1.0f);
      card.lineFrom = tgfx::Point{x + gap, y + cardHeight - gap * 1.3f};
      card.lineTo = tgfx::Point{x + cardWidth - gap * 4.6f, y + cardHeight - gap * 1.3f};
      card.textPosition = tgfx::Point{x + gap * 3.2f, y + gap * 2.0f};
      card.titleIndex = static_cast<size_t>(random.rangeInt(0, 3));
      _cards.push_back(card);
    }
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    // Background first, then a translucent wash so drawColor and drawPaint are exercised too.
    canvas->drawColor(tgfx::Color{0.94f, 0.95f, 0.97f, 1.0f});
    tgfx::Paint wash = {};
    wash.setColor(tgfx::Color{0.2f, 0.55f, 0.95f, 0.04f});
    canvas->drawPaint(wash);

    for (const auto& card : _cards) {
      auto radius = 12.0f * _density;
      canvas->drawRoundRect(card.body, radius, radius, _cardPaint);
      canvas->drawImage(_image, card.thumb.left, card.thumb.top);
      canvas->drawCircle(card.dot, 5.0f * _density, _accentPaint);
      canvas->drawOval(card.pill, _iconPaint);
      canvas->drawLine(card.lineFrom, card.lineTo, _linePaint);
      canvas->drawTextBlob(_titles[card.titleIndex], card.textPosition.x, card.textPosition.y,
                           _textPaint);

      canvas->save();
      canvas->translate(card.body.right - 34.0f * _density, card.body.top + 30.0f * _density);
      canvas->drawShape(_icon, _iconPaint);
      canvas->restore();
    }
  }

  void teardown() override {
    _cards.clear();
    _titles.clear();
    _icon = nullptr;
    _image = nullptr;
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::shared_ptr<tgfx::Image> _image = {};
  std::shared_ptr<tgfx::Shape> _icon = {};
  std::vector<std::shared_ptr<tgfx::TextBlob>> _titles = {};
  std::vector<Card> _cards = {};
  tgfx::Paint _cardPaint = {};
  tgfx::Paint _accentPaint = {};
  tgfx::Paint _linePaint = {};
  tgfx::Paint _textPaint = {};
  tgfx::Paint _iconPaint = {};
};

}  // namespace

BENCH_SCENE(DashboardScene);

}  // namespace benchmark
