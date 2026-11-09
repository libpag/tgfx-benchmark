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

#include <cmath>
#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Matrix3D.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"

namespace benchmark {
namespace {

int64_t CardsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 8;
    case Tier::M:
      return 32;
    case Tier::L:
      return 128;
    case Tier::XL:
      return 512;
  }
  return 32;
}

/// Layers rotated around the Y axis with Matrix3D, each with its own alpha and blend mode. This is
/// the layer compositing path libpag uses for 3D card flips and perspective effects.
class Layer3DScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.layer3d";
      result.category = "pag";
      result.description =
          "Layers rotated in 3D with per layer alpha and a multiply blend";
      result.realWorkload = "Card flips, perspective transitions and 3D layer transforms";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckOffscreen | BottleneckCPURecord;
      result.params = {{"cards", int64_t{32}, "number of 3D layers", true}};
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

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    Random random(0x3D000001u);
    _cards.clear();
    _cards.reserve(static_cast<size_t>(cards));
    for (int64_t i = 0; i < cards; i++) {
      auto card = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      auto width = random.range(180.0f, 360.0f) * _density;
      auto height = width * 0.62f;
      path.addRoundRect(tgfx::Rect::MakeWH(width, height), 16.0f * _density, 16.0f * _density);
      card->setPath(path);
      card->setFillStyle(tgfx::ShapeStyle::Make(
          tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 1.0f}));
      card->setAlpha(0.85f);
      card->setBlendMode(tgfx::BlendMode::Multiply);
      // setMatrix3D replaces the layer transform, so the layout translation has to live inside
      // the matrix; setting the position separately would be discarded on the first update.
      Card entry = {};
      entry.layer = card;
      // Keep every card inside the viewport, otherwise the display list culls most of them and
      // the card count stops mattering.
      const auto columns = 16;
      const auto rows = 9;
      entry.x = _width / static_cast<float>(columns) *
                static_cast<float>(i % columns);
      entry.y = _height / static_cast<float>(rows) *
                static_cast<float>(i / columns % rows);
      card->setMatrix3D(tgfx::Matrix3D::MakeTranslate(entry.x, entry.y, 0.0f));
      root->addChild(card);
      _cards.push_back(entry);
    }
    _displayList->setContentOffset(-(_width * 0.15f), -(_height * 0.1f));
    return true;
  }

  void update(int64_t frameIndex) override {
    // The rotation drives a matrix change on every card, so the layer tree re-composites each
    // frame even though the geometry is static.
    auto angle = static_cast<float>(frameIndex % 120) * 3.0f;
    for (size_t i = 0; i < _cards.size(); i++) {
      // A bounded tilt keeps the cards visible; a full spin turns them edge on and the frame
      // ends up nearly empty for a third of the rotation.
      auto tilt = std::sin(angle * 0.05f + static_cast<float>(i) * 0.7f) * 30.0f;
      // Rotate about the layer's own origin first, then move it into place; the other order
      // rotates the translated position around the document origin and throws the cards away.
      auto matrix = tgfx::Matrix3D::MakeRotate(tgfx::Vec3{0.0f, 1.0f, 0.0f}, tilt);
      matrix.postTranslate(_cards[i].x, _cards[i].y, 0.0f);
      _cards[i].layer->setMatrix3D(matrix);
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
    _cards.clear();
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
  struct Card {
    std::shared_ptr<tgfx::Layer> layer = {};
    float x = 0.0f;
    float y = 0.0f;
  };

  std::vector<Card> _cards = {};
};

}  // namespace

BENCH_SCENE(Layer3DScene);

}  // namespace benchmark
