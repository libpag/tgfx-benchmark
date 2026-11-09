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
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Matrix.h"

namespace benchmark {
namespace {

int64_t SpritesForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 10000;
    case Tier::M:
      return 60000;
    case Tier::L:
      return 150000;
    case Tier::XL:
      return 400000;
  }
  return 60000;
}

class AtlasScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.atlas";
      result.category = "prim";
      result.description =
          "Sprite batch drawn with drawAtlas from one texture, with per sprite tint";
      result.realWorkload = "Icon grids, particle systems and map markers";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckVertex | BottleneckBandwidth;
      result.params = {{"sprites", int64_t{5000}, "number of sprites per frame", true},
                       {"cell", int64_t{32}, "sprite cell size in logical pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("sprites", SpritesForTier(tier));
    params.setInt("cell", 32);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto sprites = params.getInt("sprites");
    auto cell = static_cast<float>(params.getInt("cell")) * _density;

    _atlas = sceneContext.resources->image("images/mandrill_128.png");
    if (_atlas == nullptr) {
      return false;
    }

    Random random(0xA71A5001u);
    _matrices.clear();
    _texRects.clear();
    _colors.clear();
    _matrices.reserve(static_cast<size_t>(sprites));
    _texRects.reserve(static_cast<size_t>(sprites));
    _colors.reserve(static_cast<size_t>(sprites));

    const auto atlasSize = 128.0f;
    for (int64_t i = 0; i < sprites; i++) {
      auto x = random.range(0.0f, _width - cell);
      auto y = random.range(0.0f, _height - cell);
      _matrices.push_back(tgfx::Matrix::MakeTrans(x, y));
      auto texX = random.range(0.0f, atlasSize - 32.0f);
      auto texY = random.range(0.0f, atlasSize - 32.0f);
      _texRects.push_back(tgfx::Rect::MakeXYWH(texX, texY, 32.0f, 32.0f));
      _colors.push_back(tgfx::Color{random.range(0.6f, 1.0f), random.range(0.6f, 1.0f),
                                    random.range(0.6f, 1.0f), 1.0f});
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    auto drift = static_cast<float>(frameIndex % 60) * 0.5f * _density;
    for (auto& matrix : _matrices) {
      matrix = tgfx::Matrix::MakeTrans(matrix.getTranslateX() + drift * 0.01f,
                                       matrix.getTranslateY());
    }
  }

  void draw(tgfx::Canvas* canvas) override {
    canvas->drawAtlas(_atlas, _matrices.data(), _texRects.data(), _colors.data(),
                      _matrices.size());
  }

  void teardown() override {
    _matrices.clear();
    _texRects.clear();
    _colors.clear();
    _atlas = nullptr;
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::shared_ptr<tgfx::Image> _atlas = {};
  std::vector<tgfx::Matrix> _matrices = {};
  std::vector<tgfx::Rect> _texRects = {};
  std::vector<tgfx::Color> _colors = {};
};

}  // namespace

BENCH_SCENE(AtlasScene);

}  // namespace benchmark
