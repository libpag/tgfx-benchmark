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

#include <cmath>
#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Path.h"

namespace benchmark {
namespace {

struct PathItem {
  tgfx::Path path = {};
  float x = 0.0f;
  float y = 0.0f;
};

int64_t CountForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 100;
    case Tier::M:
      return 1000;
    case Tier::L:
      return 4000;
    case Tier::XL:
      return 16000;
  }
  return 1000;
}

/// A star polygon. With vertexCount = 5 it matches the classic particle benchmark shape, larger
/// counts produce denser fills and more triangulation work.
tgfx::Path MakeStar(float radius, int32_t vertexCount, float innerRatio) {
  tgfx::Path path;
  auto points = vertexCount * 2;
  auto angleStep = static_cast<float>(M_PI) / static_cast<float>(vertexCount);
  for (auto i = 0; i < points; i++) {
    auto currentRadius = (i % 2 == 0) ? radius : radius * innerRatio;
    auto angle = static_cast<float>(i) * angleStep;
    auto x = currentRadius * std::sin(angle);
    auto y = -currentRadius * std::cos(angle);
    if (i == 0) {
      path.moveTo(x, y);
    } else {
      path.lineTo(x, y);
    }
  }
  path.close();
  return path;
}

class PathScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.path";
      result.category = "prim";
      result.description = "Concave star paths drawn through drawPath with a per draw transform";
      result.realWorkload = "Vector illustrations, icons and Lottie shape layers";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCPURecord | BottleneckVertex;
      result.params = {{"count", int64_t{2000}, "number of paths", true},
                       {"vertices", int64_t{5}, "star vertex count", false},
                       {"stroke", false, "draws the outline instead of the fill", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("count", CountForTier(tier));
    params.setInt("vertices", 5);
    params.setBool("stroke", false);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto count = params.getInt("count");
    auto vertices = static_cast<int32_t>(params.getInt("vertices"));
    auto stroke = params.getBool("stroke");

    _paint = tgfx::Paint();
    _paint.setColor(tgfx::Color{0.24f, 0.42f, 0.86f, 1.0f});
    _paint.setAntiAlias(true);
    if (stroke) {
      _paint.setStyle(tgfx::PaintStyle::Stroke);
      _paint.setStrokeWidth(2.0f * _density);
      _paint.setLineJoin(tgfx::LineJoin::Miter);
    }

    Random random(0x5A17'0001u);
    _items.clear();
    _items.reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; i++) {
      PathItem item = {};
      item.x = random.range(0.0f, _width);
      item.y = random.range(0.0f, _height);
      item.path = MakeStar(random.range(4.0f, 14.0f) * _density, vertices, 0.382f);
      _items.push_back(item);
    }
    return true;
  }

  void update(int64_t) override {
    for (auto& item : _items) {
      item.x += 1.5f * _density;
      if (item.x > _width + 32.0f) {
        item.x = -32.0f;
      }
    }
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& item : _items) {
      canvas->setMatrix(tgfx::Matrix::MakeTrans(item.x, item.y));
      canvas->drawPath(item.path, _paint);
    }
    canvas->resetMatrix();
  }

  void teardown() override {
    _items.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  tgfx::Paint _paint = {};
  std::vector<PathItem> _items = {};
};

}  // namespace

BENCH_SCENE(PathScene);

}  // namespace benchmark
