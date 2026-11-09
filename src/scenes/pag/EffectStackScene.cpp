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

#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/ColorFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/core/Shader.h"

namespace benchmark {
namespace {

int64_t ElementsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 6;
    case Tier::M:
      return 24;
    case Tier::L:
      return 72;
    case Tier::XL:
      return 200;
  }
  return 12;
}

/// One element per effect family, so a frame contains image shaders, blended shaders, composed
/// image filters, a filter blend, a corner path effect and an alpha threshold filter. This is the
/// shape of a real PAG composition: several effects stacked on a handful of layers.
class EffectStackScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.effect_stack";
      result.category = "pag";
      result.description =
          "Image and blended shaders, composed and blended image filters, corner path effect and "
          "alpha threshold, layered in one frame";
      result.realWorkload = "A motion graphics composition with several effects per layer";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckOffscreen | BottleneckFillRate;
      result.params = {{"elements", int64_t{12}, "number of effect elements", true}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("elements", ElementsForTier(tier));
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto elements = params.getInt("elements");

    auto image = sceneContext.resources->image("images/mandrill_128.png");
    if (image == nullptr) {
      return false;
    }

    auto luma = tgfx::ColorFilter::Luma();
    auto threshold = tgfx::ColorFilter::AlphaThreshold(0.4f);
    if (luma == nullptr || threshold == nullptr) {
      return false;
    }

    auto imageShader = tgfx::Shader::MakeImageShader(image, tgfx::TileMode::Repeat,
                                                     tgfx::TileMode::Repeat);
    auto gradientA = tgfx::Shader::MakeRadialGradient(
        {_width * 0.3f, _height * 0.3f}, _width * 0.4f,
        std::vector<tgfx::Color>{{1.0f, 0.4f, 0.1f, 1.0f}, {0.1f, 0.2f, 0.6f, 1.0f}});
    auto gradientB = tgfx::Shader::MakeLinearGradient(
        {0.0f, 0.0f}, {_width, _height},
        std::vector<tgfx::Color>{{0.1f, 0.8f, 0.7f, 1.0f}, {0.9f, 0.1f, 0.4f, 1.0f}});
    if (imageShader == nullptr || gradientA == nullptr || gradientB == nullptr) {
      return false;
    }

    auto blendShader = tgfx::Shader::MakeBlend(tgfx::BlendMode::Screen, gradientA, gradientB);
    auto composedFilter = tgfx::ImageFilter::Compose(
        tgfx::ImageFilter::Blur(6.0f * _density, 6.0f * _density),
        tgfx::ImageFilter::ColorFilter(luma));
    auto blendedFilter = tgfx::ImageFilter::Blend(tgfx::BlendMode::Overlay, blendShader);
    if (blendShader == nullptr || composedFilter == nullptr || blendedFilter == nullptr) {
      return false;
    }
    auto cornerEffect = tgfx::PathEffect::MakeCorner(8.0f * _density);
    if (cornerEffect == nullptr) {
      return false;
    }

    // Six effect families, cycled.
    _variants.clear();
    _variants.push_back(MakeVariant(imageShader, nullptr, nullptr));
    _variants.push_back(MakeVariant(blendShader, nullptr, nullptr));
    _variants.push_back(MakeVariant(nullptr, composedFilter, nullptr));
    _variants.push_back(MakeVariant(nullptr, blendedFilter, nullptr));
    _variants.push_back(MakeVariant(nullptr, tgfx::ImageFilter::ColorFilter(threshold), nullptr));
    _variants.push_back(MakeVariant(nullptr, nullptr, cornerEffect));

    Random random(0xEFFE0001u);
    _elements.clear();
    _elements.reserve(static_cast<size_t>(elements));
    for (int64_t i = 0; i < elements; i++) {
      Element element = {};
      auto size = random.range(200.0f, 420.0f) * _density;
      element.rect = tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                          random.range(0.0f, _height - size), size, size);
      element.variant = static_cast<size_t>(i) % _variants.size();
      element.stroke = element.variant == 5;
      _elements.push_back(element);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    _shift = static_cast<float>(frameIndex % 90) * 0.4f;
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& element : _elements) {
      const auto& variant = _variants[element.variant];
      if (variant.effect != nullptr) {
        auto path = tgfx::Path();
        path.addRect(element.rect);
        if (variant.effect->filterPath(&path)) {
          tgfx::Paint paint = {};
          paint.setStyle(tgfx::PaintStyle::Stroke);
          paint.setStrokeWidth(6.0f * _density);
          paint.setColor(tgfx::Color{0.95f, 0.65f, 0.2f, 1.0f});
          paint.setAntiAlias(true);
          canvas->drawPath(path, paint);
        }
        continue;
      }
      tgfx::Paint paint = {};
      if (variant.shader != nullptr) {
        canvas->save();
        canvas->translate(_shift, 0.0f);
        paint.setShader(variant.shader);
        canvas->drawRect(element.rect, paint);
        canvas->restore();
      } else if (variant.filter != nullptr) {
        paint.setImageFilter(variant.filter);
        canvas->drawRect(element.rect, paint);
      }
    }
  }

  void teardown() override {
    _elements.clear();
    _variants.clear();
  }

 private:
  struct Variant {
    std::shared_ptr<tgfx::Shader> shader = {};
    std::shared_ptr<tgfx::ImageFilter> filter = {};
    std::shared_ptr<tgfx::PathEffect> effect = {};
  };

  struct Element {
    tgfx::Rect rect = {};
    size_t variant = 0;
    bool stroke = false;
  };

  static Variant MakeVariant(std::shared_ptr<tgfx::Shader> shader,
                             std::shared_ptr<tgfx::ImageFilter> filter,
                             std::shared_ptr<tgfx::PathEffect> effect) {
    Variant variant = {};
    variant.shader = std::move(shader);
    variant.filter = std::move(filter);
    variant.effect = std::move(effect);
    return variant;
  }

  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  float _shift = 0.0f;
  std::vector<Variant> _variants = {};
  std::vector<Element> _elements = {};
};

}  // namespace

BENCH_SCENE(EffectStackScene);

}  // namespace benchmark
