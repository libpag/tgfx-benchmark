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
#include "tgfx/layers/layerstyles/BackgroundBlurStyle.h"

namespace benchmark {
namespace {

int64_t PanelsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1;
    case Tier::M:
      return 4;
    case Tier::L:
      return 16;
    case Tier::XL:
      return 64;
  }
  return 4;
}

class GlassScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "ui.glass";
      result.category = "ui";
      result.description =
          "Frosted panels using a background blur style over continuously moving content";
      result.realWorkload = "Modern translucent UI panels, sheets and navigation bars";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckOffscreen | BottleneckFillRate;
      result.params = {{"panels", int64_t{4}, "number of glass panels", true},
                       {"blur", 20.0, "background blur sigma in logical pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("panels", PanelsForTier(tier));
    params.setDouble("blur", 20.0);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto panels = params.getInt("panels");
    auto blur = static_cast<float>(params.getDouble("blur")) * _density;

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    Random random(0x61A55001u);
    _backdrop.clear();
    for (auto i = 0; i < 48; i++) {
      auto shape = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      auto size = random.range(120.0f, 420.0f) * _density;
      path.addOval(tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                        random.range(0.0f, _height - size), size, size));
      shape->setPath(path);
      shape->setFillStyle(tgfx::ShapeStyle::Make(
          tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 0.9f}));
      root->addChild(shape);
      _backdrop.push_back(shape);
    }

    auto glassStyle = tgfx::BackgroundBlurStyle::Make(blur, blur);
    if (glassStyle == nullptr) {
      return false;
    }
    _panels.clear();
    for (int64_t i = 0; i < panels; i++) {
      auto panel = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      auto panelWidth = random.range(220.0f, 520.0f) * _density;
      auto panelHeight = panelWidth * random.range(0.4f, 0.8f);
      path.addRoundRect(tgfx::Rect::MakeXYWH(random.range(0.0f, _width - panelWidth),
                                             random.range(0.0f, _height - panelHeight),
                                             panelWidth, panelHeight),
                        24.0f * _density, 24.0f * _density);
      panel->setPath(path);
      panel->setFillStyle(
          tgfx::ShapeStyle::Make(tgfx::Color{1.0f, 1.0f, 1.0f, 0.35f}));
      panel->setLayerStyles({glassStyle});
      root->addChild(panel);
      _panels.push_back(panel);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    auto phase = static_cast<float>(frameIndex % 180) / 180.0f;
    auto angle = phase * 2.0f * static_cast<float>(M_PI);
    for (size_t i = 0; i < _backdrop.size(); i++) {
      auto offset = static_cast<float>(i) * 0.35f + angle;
      _backdrop[i]->setPosition(tgfx::Point{std::cos(offset) * 60.0f * _density,
                                            std::sin(offset) * 40.0f * _density});
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
    _backdrop.clear();
    _panels.clear();
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
  std::vector<std::shared_ptr<tgfx::Layer>> _backdrop = {};
  std::vector<std::shared_ptr<tgfx::Layer>> _panels = {};
};

}  // namespace

BENCH_SCENE(GlassScene);

}  // namespace benchmark
