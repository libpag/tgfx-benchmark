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
#include <algorithm>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Shader.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"

namespace benchmark {
namespace {

int64_t PairsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 8;
    case Tier::M:
      return 48;
    case Tier::L:
      return 160;
    case Tier::XL:
      return 512;
  }
  return 8;
}

struct MattePair {
  std::shared_ptr<tgfx::Layer> content = {};
  std::shared_ptr<tgfx::Layer> mask = {};
  tgfx::Point origin = {};
};

/// Track matte the way libpag expresses it: two sibling layers, where the mask layer's alpha or
/// luminance cuts the content layer. This is a different code path from MaskFilter on a Canvas
/// draw, so it needs its own scene.
class MatteLayersScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.matte_layers";
      result.category = "pag";
      result.description =
          "Layer pairs where a mask layer cuts the content layer by alpha or luminance";
      result.realWorkload = "libpag track mattes, the most common masking form in motion graphics";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckOffscreen | BottleneckFillRate;
      result.params = {{"pairs", int64_t{8}, "content and mask layer pairs", true}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("pairs", PairsForTier(tier));
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto pairs = params.getInt("pairs");

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    Random random(0xA77E0001u);
    // A grid that shrinks with the pair count keeps every pair on screen; a fixed cell size
    // pushed the extra pairs out of view and the ladder saturated.
    auto columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(pairs))));
    if (columns < 1) {
      columns = 1;
    }
    auto rows = static_cast<int>((pairs + columns - 1) / columns);
    auto cell = std::min(_width / static_cast<float>(columns),
                         _height / static_cast<float>(rows));
    _pairs.clear();
    _pairs.reserve(static_cast<size_t>(pairs));
    for (int64_t i = 0; i < pairs; i++) {
      MattePair pair = {};
      pair.origin = tgfx::Point{cell * static_cast<float>(i % columns),
                                cell * static_cast<float>(i / columns)};

      auto content = tgfx::Layer::Make();
      content->setPosition(pair.origin);
      auto size = cell * 0.8f;
      for (auto k = 0; k < 3; k++) {
        auto shape = tgfx::ShapeLayer::Make();
        tgfx::Path path;
        path.addRect(tgfx::Rect::MakeXYWH(static_cast<float>(k) * size * 0.25f,
                                          static_cast<float>(k) * size * 0.25f,
                                          size * 0.6f, size * 0.6f));
        shape->setPath(path);
        shape->setFillStyle(tgfx::ShapeStyle::Make(
            tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 1.0f}));
        content->addChild(shape);
      }

      auto mask = tgfx::Layer::Make();
      mask->setPosition(pair.origin);
      auto maskShape = tgfx::ShapeLayer::Make();
      tgfx::Path maskPath;
      auto radius = size * 0.5f;
      maskPath.addOval(tgfx::Rect::MakeXYWH(size * 0.5f - radius, size * 0.5f - radius,
                                            radius * 2.0f, radius * 2.0f));
      maskShape->setPath(maskPath);
      // A soft radial ramp, so the mask actually has a gradient to cut with.
      auto colors = std::vector<tgfx::Color>{{1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 0.0f}};
      auto shader = tgfx::Shader::MakeRadialGradient(
          {size * 0.5f, size * 0.5f}, radius, colors);
      if (shader == nullptr) {
        return false;
      }
      maskShape->setFillStyle(tgfx::ShapeStyle::Make(shader));
      mask->addChild(maskShape);

      content->setMask(mask);
      content->setMaskType((i % 2 == 0) ? tgfx::LayerMaskType::Alpha
                                       : tgfx::LayerMaskType::Luminance);
      // The mask layer has to live in the active part of the display list, but it is not drawn.
      root->addChild(content);
      root->addChild(mask);
      pair.content = content;
      pair.mask = mask;
      _pairs.push_back(pair);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    // Moving the mask changes the cut every frame, which is what a matte animation costs.
    auto drift = static_cast<float>(frameIndex % 90) * 0.4f * _density;
    for (auto& pair : _pairs) {
      pair.mask->setPosition(tgfx::Point{pair.origin.x + drift, pair.origin.y});
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
    for (auto& pair : _pairs) {
      pair.content->setMask(nullptr);
    }
    _pairs.clear();
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
  std::vector<MattePair> _pairs = {};
};

}  // namespace

BENCH_SCENE(MatteLayersScene);

}  // namespace benchmark
