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
//  license is distributed on an "AS IS" BASIS, without warranties or conditions of any kind,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the license.
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

int64_t EditsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1;
    case Tier::M:
      return 10;
    case Tier::L:
      return 100;
    case Tier::XL:
      return 1000;
  }
  return 10;
}

class EditScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "doc.edit";
      result.category = "doc";
      result.description =
          "A large tiled document where a growing number of layers is modified every frame";
      result.realWorkload = "Dragging a selection or dragging a colour picker across a canvas";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckCache | BottleneckCPURecord;
      result.params = {{"edits", int64_t{10}, "layers modified per frame", true},
                       {"layers", int64_t{20000}, "total leaf layers", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("edits", EditsForTier(tier));
    params.setInt("layers", 20000);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _edits = params.getInt("edits");
    auto layerCount = params.getInt("layers");

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Tiled);
    _displayList->setTileSize(256);
    _displayList->setMaxTileCount(4096);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});

    _documentWidth = _width * 1.5f;
    _documentHeight = _height * 1.5f;
    _leaves.clear();
    if (!BuildDocument(_displayList->root(), sceneContext, layerCount, 0xED170001u,
                       _documentWidth, _documentHeight, &_leaves)) {
      return false;
    }
    // Keep the camera still: the point is the invalidation cost of the edits, not panning.
    _displayList->setContentOffset(-(_documentWidth - _width) * 0.5f,
                                   -(_documentHeight - _height) * 0.5f);
    return true;
  }

  void update(int64_t frameIndex) override {
    if (_leaves.empty() || _edits <= 0) {
      return;
    }
    Random random(static_cast<uint64_t>(frameIndex) * 0x9E3779B97F4A7C15ULL + 1U);
    for (int64_t i = 0; i < _edits; i++) {
      auto index = static_cast<size_t>(random.rangeInt(0, static_cast<int32_t>(_leaves.size() - 1)));
      auto shape = std::static_pointer_cast<tgfx::ShapeLayer>(_leaves[index]);
      if (shape == nullptr) {
        continue;
      }
      // A new immutable style each time mirrors how an editor applies a colour change.
      shape->setFillStyle(tgfx::ShapeStyle::Make(
          tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 0.9f}));
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
    _leaves.clear();
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
  float _documentWidth = 0.0f;
  float _documentHeight = 0.0f;
  int64_t _edits = 0;
  std::unique_ptr<tgfx::DisplayList> _displayList = {};
  std::vector<std::shared_ptr<tgfx::Layer>> _leaves = {};
};

}  // namespace

BENCH_SCENE(EditScene);

}  // namespace benchmark
