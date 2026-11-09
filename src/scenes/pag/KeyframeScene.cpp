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
//  either express or implied. See the License for the specific language governing permissions
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

int64_t LayersForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1600;
    case Tier::M:
      return 10000;
    case Tier::L:
      return 30000;
    case Tier::XL:
      return 100000;
  }
  return 10000;
}

struct Animated {
  std::shared_ptr<tgfx::Layer> layer = {};
  tgfx::Point base = {};
  float phase = 0.0f;
  float scale = 1.0f;
};

/// A composition timeline: every layer gets a new position, rotation, scale and alpha each frame,
/// which is the dominant CPU cost of playback even when nothing new is rasterized.
class KeyframeScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.keyframes";
      result.category = "pag";
      result.description =
          "Every layer gets new position, rotation, scale and alpha on every frame";
      result.realWorkload = "Timeline playback in libpag compositions";
      result.modules = ModuleLayers;
      result.bottlenecks = BottleneckCPURecord | BottleneckCache;
      result.params = {{"layers", int64_t{400}, "animated layers", true},
                       {"properties", int64_t{4}, "animated properties per layer", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    params.setInt("properties", 4);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto layers = params.getInt("layers");
    _properties = static_cast<int>(params.getInt("properties"));

    _displayList = std::make_unique<tgfx::DisplayList>();
    _displayList->setRenderMode(tgfx::RenderMode::Direct);
    _displayList->setBackgroundColor(tgfx::Color{0.87f, 0.87f, 0.87f, 1.0f});
    auto root = _displayList->root();
    if (root == nullptr) {
      return false;
    }

    Random random(0x4E7F0001u);
    _layers.clear();
    _layers.reserve(static_cast<size_t>(layers));
    for (int64_t i = 0; i < layers; i++) {
      auto layer = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      auto size = random.range(24.0f, 64.0f) * _density;
      path.addRoundRect(tgfx::Rect::MakeWH(size, size), 6.0f * _density, 6.0f * _density);
      layer->setPath(path);
      layer->setFillStyle(tgfx::ShapeStyle::Make(
          tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 0.9f}));
      Animated entry = {};
      entry.base = tgfx::Point{random.range(0.0f, _width), random.range(0.0f, _height)};
      entry.phase = random.range(0.0f, 6.2831853f);
      entry.scale = random.range(0.6f, 1.4f);
      layer->setPosition(entry.base);
      root->addChild(layer);
      entry.layer = layer;
      _layers.push_back(entry);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    auto t = static_cast<float>(frameIndex % 120) * 0.05f;
    for (auto& entry : _layers) {
      if (_properties >= 1) {
        entry.layer->setPosition(tgfx::Point{entry.base.x + std::sin(t + entry.phase) * 20.0f,
                                             entry.base.y + std::cos(t + entry.phase) * 20.0f});
      }
      if (_properties >= 2) {
        entry.layer->setAlpha(0.6f + 0.4f * std::sin(t * 1.3f + entry.phase));
      }
      if (_properties >= 3) {
        auto spin = std::sin(t * 0.7f + entry.phase) * 45.0f;
        entry.layer->setMatrix(tgfx::Matrix::MakeRotate(spin) *
                               tgfx::Matrix::MakeScale(entry.scale, entry.scale));
      }
      if (_properties >= 4) {
        auto tilt = std::sin(t * 0.4f + entry.phase) * 25.0f;
        auto matrix = tgfx::Matrix3D::MakeRotate(tgfx::Vec3{0.0f, 1.0f, 0.0f}, tilt);
        matrix.postTranslate(entry.base.x, entry.base.y, 0.0f);
        entry.layer->setMatrix3D(matrix);
      }
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
  int _properties = 4;
  std::unique_ptr<tgfx::DisplayList> _displayList = {};
  std::vector<Animated> _layers = {};
};

}  // namespace

BENCH_SCENE(KeyframeScene);

}  // namespace benchmark
