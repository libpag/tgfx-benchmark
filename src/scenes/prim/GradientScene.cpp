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
#include "core/SceneRegistry.h"
#include "tgfx/core/Shader.h"

namespace benchmark {
namespace {

constexpr size_t ShaderCount = 4;

struct GradientRect {
  tgfx::Rect rect = {};
  size_t shaderIndex = 0;
};

int64_t CountForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 16;
    case Tier::M:
      return 128;
    case Tier::L:
      return 512;
    case Tier::XL:
      return 2048;
  }
  return 128;
}

std::vector<tgfx::Color> MakeColors(size_t stopCount) {
  std::vector<tgfx::Color> colors;
  colors.reserve(stopCount);
  Random random(0x51ED270Bu ^ static_cast<uint32_t>(stopCount));
  for (size_t i = 0; i < stopCount; i++) {
    tgfx::Color color = {random.nextFloat(), random.nextFloat(), random.nextFloat(), 1.0f};
    colors.push_back(color);
  }
  return colors;
}

class GradientScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.gradient";
      result.category = "prim";
      result.description = "Large gradient filled rects cycling linear, radial and conic shaders";
      result.realWorkload = "Backgrounds, buttons and progress indicators using gradient shaders";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckFillRate;
      result.params = {{"count", int64_t{128}, "number of gradient rects", true},
                       {"stops", int64_t{4}, "gradient color stop count", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("count", CountForTier(tier));
    params.setInt("stops", 4);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto count = params.getInt("count");
    auto stopCount = static_cast<size_t>(params.getInt("stops"));

    auto colors = MakeColors(stopCount);
    auto radius = 256.0f * _density;
    _shaders[0] = tgfx::Shader::MakeLinearGradient({0.0f, 0.0f}, {radius, radius}, colors);
    _shaders[1] =
        tgfx::Shader::MakeRadialGradient({radius * 0.5f, radius * 0.5f}, radius, colors);
    _shaders[2] = tgfx::Shader::MakeConicGradient({radius * 0.5f, radius * 0.5f}, 0.0f, 360.0f,
                                                  colors);
    _shaders[3] = tgfx::Shader::MakeDiamondGradient({radius * 0.5f, radius * 0.5f}, radius, colors);
    for (auto& shader : _shaders) {
      if (shader == nullptr) {
        return false;
      }
    }

    Random random(0xC0FFEE01u);
    _rects.clear();
    _rects.reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; i++) {
      GradientRect item = {};
      auto size = random.range(180.0f, 420.0f) * _density;
      item.rect = tgfx::Rect::MakeXYWH(random.range(0.0f, _width) - size * 0.5f,
                                       random.range(0.0f, _height) - size * 0.5f, size, size);
      item.shaderIndex = static_cast<size_t>(i) % ShaderCount;
      _rects.push_back(item);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    auto offset = static_cast<float>(frameIndex % 120) * 2.0f * _density;
    for (auto& item : _rects) {
      item.rect.offsetTo(item.rect.left + offset * 0.05f, item.rect.top);
    }
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& item : _rects) {
      _paint.setShader(_shaders[item.shaderIndex]);
      canvas->drawRect(item.rect, _paint);
    }
  }

  void teardown() override {
    _rects.clear();
    for (auto& shader : _shaders) {
      shader = nullptr;
    }
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  tgfx::Paint _paint = {};
  std::shared_ptr<tgfx::Shader> _shaders[ShaderCount] = {};
  std::vector<GradientRect> _rects = {};
};

}  // namespace

BENCH_SCENE(GradientScene);

}  // namespace benchmark
