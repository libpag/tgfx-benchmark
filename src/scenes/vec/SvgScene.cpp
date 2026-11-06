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
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Stream.h"
#include "tgfx/svg/SVGDOM.h"

namespace benchmark {
namespace {

int64_t CopiesForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1;
    case Tier::M:
      return 2;
    case Tier::L:
      return 4;
    case Tier::XL:
      return 8;
  }
  return 4;
}

class SvgScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "vec.svg";
      result.category = "vec";
      result.description = "A large SVG document rendered repeatedly with a translate transform";
      result.realWorkload = "Illustrations and icon sheets drawn from SVG sources";
      result.modules = ModuleSVG;
      result.bottlenecks = BottleneckCPURecord;
      result.params = {{"copies", int64_t{4}, "number of SVG instances per frame", true},
                       {"file", std::string("svg/complex5.svg"), "svg resource path", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("copies", CopiesForTier(tier));
    params.setString("file", "svg/complex5.svg");
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    _copies = params.getInt("copies");
    auto file = params.getString("file");

    auto stream = tgfx::Stream::MakeFromFile(sceneContext.resources->path(file));
    if (stream == nullptr) {
      return false;
    }
    _svgDom = tgfx::SVGDOM::Make(*stream);
    if (_svgDom == nullptr) {
      return false;
    }
    _svgDom->setContainerSize(tgfx::Size{256.0f * _density, 256.0f * _density});
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    const auto columns = 8;
    const auto cell = _width / static_cast<float>(columns);
    for (int64_t i = 0; i < _copies; i++) {
      auto column = static_cast<int>(i % columns);
      auto row = static_cast<int>(i / columns);
      auto x = static_cast<float>(column) * cell;
      auto y = static_cast<float>(row) * cell;
      if (y > _height) {
        break;
      }
      canvas->save();
      canvas->translate(x, y);
      _svgDom->render(canvas);
      canvas->restore();
    }
  }

  void teardown() override {
    _svgDom = nullptr;
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  int64_t _copies = 0;
  std::shared_ptr<tgfx::SVGDOM> _svgDom = {};
};

}  // namespace

BENCH_SCENE(SvgScene);

}  // namespace benchmark
