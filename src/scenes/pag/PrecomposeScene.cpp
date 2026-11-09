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
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"

namespace benchmark {
namespace {

int64_t DepthForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 4;
    case Tier::M:
      return 16;
    case Tier::L:
      return 40;
    case Tier::XL:
      return 100;
  }
  return 16;
}

/// Precompositions: groups nested inside groups, each with its own alpha and blend, which forces
/// one offscreen target per level and a composition pass on the way back up.
class PrecomposeScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.precompose";
      result.category = "pag";
      result.description =
          "Nested group layers, each with alpha and a blend mode, over a handful of leaf shapes";
      result.realWorkload = "Nested precompositions and group opacity in motion graphics";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckOffscreen | BottleneckCPURecord;
      result.params = {{"depth", int64_t{8}, "nesting depth", true},
                       {"leaves", int64_t{6}, "leaf shapes per level", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("depth", DepthForTier(tier));
    params.setInt("leaves", 6);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto depth = params.getInt("depth");
    auto leaves = params.getInt("leaves");

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    Random random(0x9CE00001u);
    _groups.clear();
    tgfx::Layer* parent = root;
    for (int64_t level = 0; level < depth; level++) {
      auto group = tgfx::Layer::Make();
      group->setAlpha(0.95f);
      group->setBlendMode((level % 3 == 0) ? tgfx::BlendMode::SrcOver
                                           : tgfx::BlendMode::Multiply);
      group->setPosition(tgfx::Point{12.0f * _density, 10.0f * _density});
      for (int64_t i = 0; i < leaves; i++) {
        auto shape = tgfx::ShapeLayer::Make();
        tgfx::Path path;
        auto size = random.range(80.0f, 220.0f) * _density;
        path.addOval(tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                          random.range(0.0f, _height - size), size, size));
        shape->setPath(path);
        shape->setFillStyle(tgfx::ShapeStyle::Make(
            tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 0.9f}));
        group->addChild(shape);
      }
      parent->addChild(group);
      _groups.push_back(group);
      parent = group.get();
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    // Rotating each level independently invalidates the whole chain every frame.
    auto angle = static_cast<float>(frameIndex % 180) * 0.5f;
    for (size_t i = 0; i < _groups.size(); i++) {
      auto spin = angle * (0.2f + static_cast<float>(i) * 0.05f);
      _groups[i]->setMatrix(tgfx::Matrix::MakeRotate(spin));
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
    _groups.clear();
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
  std::vector<std::shared_ptr<tgfx::Layer>> _groups = {};
};

}  // namespace

BENCH_SCENE(PrecomposeScene);

}  // namespace benchmark
