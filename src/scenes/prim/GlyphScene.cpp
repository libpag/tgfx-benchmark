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
//  license is distributed on an "as is" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Font.h"

namespace benchmark {
namespace {

int64_t GlyphsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 20000;
    case Tier::M:
      return 150000;
    case Tier::L:
      return 400000;
    case Tier::XL:
      return 1000000;
  }
  return 150000;
}

/// drawGlyphs takes pre-shaped glyph ids and positions, which is the path animated typography and
/// text heavy UI use after layout has already happened.
class GlyphScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.glyphs";
      result.category = "prim";
      result.description = "Pre-shaped glyph ids drawn with drawGlyphs at explicit positions";
      result.realWorkload = "Animated typography and text that has already been laid out";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckVertex | BottleneckCPURecord;
      result.params = {{"glyphs", int64_t{20000}, "glyphs per frame", true},
                       {"size", 20.0, "font size in logical pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("glyphs", GlyphsForTier(tier));
    params.setDouble("size", 20.0);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto glyphCount = params.getInt("glyphs");
    auto size = static_cast<float>(params.getDouble("size")) * _density;

    auto typeface = sceneContext.resources->typeface("font/NotoSansSC-Regular.otf");
    if (typeface == nullptr) {
      return false;
    }
    _font = tgfx::Font(typeface, size);
    _paint = tgfx::Paint();
    _paint.setColor(tgfx::Color{0.12f, 0.13f, 0.17f, 1.0f});
    _paint.setAntiAlias(true);

    const std::string corpus = "The quick brown fox jumps over 0123456789";
    std::vector<tgfx::GlyphID> ids;
    ids.reserve(corpus.size());
    for (auto character : corpus) {
      auto glyph = _font.getGlyphID(static_cast<tgfx::Unichar>(character));
      if (glyph != 0) {
        ids.push_back(glyph);
      }
    }
    if (ids.empty()) {
      return false;
    }

    Random random(0x61F00001u);
    auto advance = size * 0.6f;
    auto columns = static_cast<int64_t>(_width / advance);
    if (columns < 1) {
      columns = 1;
    }
    _glyphs.clear();
    _positions.clear();
    _glyphs.reserve(static_cast<size_t>(glyphCount));
    _positions.reserve(static_cast<size_t>(glyphCount));
    for (int64_t i = 0; i < glyphCount; i++) {
      _glyphs.push_back(ids[static_cast<size_t>(i) % ids.size()]);
      auto x = static_cast<float>(i % columns) * advance;
      auto y = static_cast<float>(i / columns) * size * 1.2f;
      if (y > _height) {
        y = std::fmod(y, _height);
      }
      _positions.push_back(tgfx::Point{x, y});
    }
    (void)random;
    return true;
  }

  void update(int64_t frameIndex) override {
    // A tiny horizontal shift defeats the raster cache without changing the shape count.
    _shift = static_cast<float>(frameIndex % 60) * 0.25f;
  }

  void draw(tgfx::Canvas* canvas) override {
    if (_glyphs.empty()) {
      return;
    }
    canvas->save();
    canvas->translate(_shift, 0.0f);
    canvas->drawGlyphs(_glyphs.data(), _positions.data(), _glyphs.size(), _font, _paint);
    canvas->restore();
  }

  void teardown() override {
    _glyphs.clear();
    _positions.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  float _shift = 0.0f;
  tgfx::Font _font = {};
  tgfx::Paint _paint = {};
  std::vector<tgfx::GlyphID> _glyphs = {};
  std::vector<tgfx::Point> _positions = {};
};

}  // namespace

BENCH_SCENE(GlyphScene);

}  // namespace benchmark
