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

#include <vector>
#include "core/Random.h"
#include "core/SceneRegistry.h"

namespace benchmark {
namespace {

struct Particle {
  float x = 0.0f;
  float y = 0.0f;
  float size = 8.0f;
  float speedX = 0.0f;
  float speedY = 0.0f;
};

int64_t CountForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 2000;
    case Tier::M:
      return 20000;
    case Tier::L:
      return 80000;
    case Tier::XL:
      return 300000;
  }
  return 20000;
}

class RectScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.rect";
      result.category = "prim";
      result.description = "Solid axis aligned rectangles with three rotating colors";
      result.realWorkload = "Baseline for canvas fill throughput and draw call batching";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCPURecord | BottleneckVertex;
      result.params = {{"count", int64_t{20000}, "number of rects", true},
                       {"size", 8.0, "average edge length in logical pixels", false},
                       {"antialias", true, "enables paint anti aliasing", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("count", CountForTier(tier));
    params.setDouble("size", 8.0);
    params.setBool("antialias", true);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    _count = params.getInt("count");
    auto averageSize = static_cast<float>(params.getDouble("size")) * _density;
    auto antialias = params.getBool("antialias");

    for (auto i = 0; i < 3; i++) {
      tgfx::Color color = tgfx::Color::Black();
      color[i] = 1.0f;
      _paints[i] = tgfx::Paint();
      _paints[i].setColor(color);
      _paints[i].setAntiAlias(antialias);
    }

    Random random(0x9E3779B97F4A7C15ULL ^ static_cast<uint64_t>(_count));
    _particles.clear();
    _particles.reserve(static_cast<size_t>(_count));
    for (int64_t i = 0; i < _count; i++) {
      Particle particle = {};
      particle.x = random.range(0.0f, _width);
      particle.y = random.range(0.0f, _height);
      particle.size = averageSize * random.range(0.5f, 1.5f);
      particle.speedX = random.range(-5.0f, 5.0f) * _density;
      particle.speedY = random.range(-5.0f, 5.0f) * _density;
      _particles.push_back(particle);
    }
    return true;
  }

  void update(int64_t) override {
    for (auto& particle : _particles) {
      particle.x += particle.speedX;
      particle.y += particle.speedY;
      if (particle.x < -particle.size) {
        particle.x = _width + particle.size;
      } else if (particle.x > _width + particle.size) {
        particle.x = -particle.size;
      }
      if (particle.y < -particle.size) {
        particle.y = _height + particle.size;
      } else if (particle.y > _height + particle.size) {
        particle.y = -particle.size;
      }
    }
  }

  void draw(tgfx::Canvas* canvas) override {
    auto index = 0;
    for (const auto& particle : _particles) {
      auto rect = tgfx::Rect::MakeXYWH(particle.x, particle.y, particle.size, particle.size);
      canvas->drawRect(rect, _paints[static_cast<size_t>(index) % 3]);
      index++;
    }
  }

  void teardown() override {
    _particles.clear();
    _particles.shrink_to_fit();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  int64_t _count = 0;
  tgfx::Paint _paints[3] = {};
  std::vector<Particle> _particles = {};
};

}  // namespace

BENCH_SCENE(RectScene);

}  // namespace benchmark
