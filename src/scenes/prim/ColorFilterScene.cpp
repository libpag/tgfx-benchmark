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
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/ColorFilter.h"

namespace benchmark {
namespace {

int64_t CountForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 200;
    case Tier::M:
      return 2000;
    case Tier::L:
      return 8000;
    case Tier::XL:
      return 32000;
  }
  return 2000;
}

class ColorFilterScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.color_filter";
      result.category = "prim";
      result.description =
          "Images and shapes drawn through blend, matrix, luma and composed colour filters";
      result.realWorkload = "Tinting, recolouring and masking artwork in editors and players";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckFillRate | BottleneckVertex;
      result.params = {{"count", int64_t{2000}, "draws per frame", true}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("count", CountForTier(tier));
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto count = params.getInt("count");

    _image = sceneContext.resources->image("images/mandrill_128.png");
    if (_image == nullptr) {
      return false;
    }

    const std::array<float, 20> matrix = {
        0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.2f, 0.0f, 0.0f,
        0.1f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    };
    _filters.clear();
    _filters.push_back(tgfx::ColorFilter::Blend(tgfx::Color{0.9f, 0.5f, 0.2f, 1.0f},
                                                tgfx::BlendMode::Multiply));
    _filters.push_back(tgfx::ColorFilter::Matrix(matrix));
    _filters.push_back(tgfx::ColorFilter::Luma());
    _filters.push_back(tgfx::ColorFilter::Compose(
        tgfx::ColorFilter::Luma(),
        tgfx::ColorFilter::Blend(tgfx::Color{0.4f, 0.7f, 1.0f, 1.0f}, tgfx::BlendMode::Screen)));
    for (const auto& filter : _filters) {
      if (filter == nullptr) {
        return false;
      }
    }

    Random random(0xC0100001u);
    _items.clear();
    _items.reserve(static_cast<size_t>(count));
    for (int64_t i = 0; i < count; i++) {
      Item item = {};
      auto size = random.range(48.0f, 160.0f) * _density;
      item.rect = tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                       random.range(0.0f, _height - size), size, size);
      item.filterIndex = static_cast<size_t>(i) % _filters.size();
      item.paint = tgfx::Paint();
      item.paint.setColorFilter(_filters[item.filterIndex]);
      item.paint.setAntiAlias(true);
      _items.push_back(item);
    }
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& item : _items) {
      // Images exercise the filter on texture fetches, rects on flat colour.
      if (item.filterIndex % 2 == 0) {
        canvas->drawImageRect(_image, item.rect, tgfx::SamplingOptions(), &item.paint);
      } else {
        canvas->drawRRect(tgfx::RRect::MakeRectXY(item.rect, 10.0f, 10.0f), item.paint);
      }
    }
  }

  void teardown() override {
    _items.clear();
    _filters.clear();
    _image = nullptr;
  }

 private:
  struct Item {
    tgfx::Rect rect = {};
    tgfx::Paint paint = {};
    size_t filterIndex = 0;
  };

  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::shared_ptr<tgfx::Image> _image = {};
  std::vector<std::shared_ptr<tgfx::ColorFilter>> _filters = {};
  std::vector<Item> _items = {};
};

}  // namespace

BENCH_SCENE(ColorFilterScene);

}  // namespace benchmark
