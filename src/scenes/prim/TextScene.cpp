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
#include <string>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Font.h"

namespace benchmark {
namespace {

const char* const LatinCorpus[] = {
    "The quick brown fox jumps over the lazy dog",
    "Graphics pipelines prefer batched submissions",
    "Rasterize, composite, present, repeat",
};

const char* const CjkCorpus[] = {
    "腾讯图形引擎在移动端与浏览器上的渲染性能",
    "矢量绘制与位图合成需要在同一帧内完成",
    "文本排版与字形缓存是界面渲染的常见热点",
};

struct TextRun {
  std::string text = {};
  float x = 0.0f;
  float y = 0.0f;
  size_t fontIndex = 0;
};

int64_t CountForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 250;
    case Tier::M:
      return 1000;
    case Tier::L:
      return 4000;
    case Tier::XL:
      return 16000;
  }
  return 1000;
}

class TextScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.text";
      result.category = "prim";
      result.description = "Short text runs with four font sizes, latin or CJK corpus";
      result.realWorkload = "Labels, chat bubbles and document text laid out every frame";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCPURecord | BottleneckCache;
      result.params = {{"runs", int64_t{20000}, "number of text runs", true},
                       {"script", std::string("cjk"), "latin or cjk corpus", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("runs", CountForTier(tier));
    params.setString("script", "cjk");
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto runs = params.getInt("runs");
    auto script = params.getString("script");

    auto typeface = sceneContext.resources->typeface("font/NotoSansSC-Regular.otf");
    if (typeface == nullptr) {
      return false;
    }
    const float baseSizes[4] = {12.0f, 18.0f, 28.0f, 44.0f};
    for (size_t i = 0; i < 4; i++) {
      _fonts[i] = tgfx::Font(typeface, baseSizes[i] * _density);
    }

    _paint = tgfx::Paint();
    _paint.setColor(tgfx::Color{0.1f, 0.12f, 0.16f, 1.0f});
    _paint.setAntiAlias(true);

    Random random(script == "latin" ? 0x7E47'0001u : 0x7E47'0002u);
    _runs.clear();
    _runs.reserve(static_cast<size_t>(runs));
    for (int64_t i = 0; i < runs; i++) {
      TextRun run = {};
      if (script == "latin") {
        run.text = LatinCorpus[random.rangeInt(0, 2)];
      } else {
        run.text = CjkCorpus[random.rangeInt(0, 2)];
      }
      run.x = random.range(0.0f, _width - 400.0f * _density);
      run.y = random.range(0.0f, _height);
      run.fontIndex = static_cast<size_t>(random.rangeInt(0, 3));
      _runs.push_back(run);
    }
    return true;
  }

  void update(int64_t) override {
    // Static text: layout, glyph cache lookups and rasterization dominate, not animation.
  }

  void draw(tgfx::Canvas* canvas) override {
    for (const auto& run : _runs) {
      canvas->drawSimpleText(run.text, run.x, run.y, _fonts[run.fontIndex], _paint);
    }
  }

  void teardown() override {
    _runs.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  tgfx::Paint _paint = {};
  tgfx::Font _fonts[4] = {};
  std::vector<TextRun> _runs = {};
};

}  // namespace

BENCH_SCENE(TextScene);

}  // namespace benchmark
