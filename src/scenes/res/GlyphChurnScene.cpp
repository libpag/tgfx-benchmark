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

const char* const ChurnCorpus[] = {
    "glyph atlas churn stress",
    "字形缓存抖动测试",
    "mixed 中英 text 0123",
    "abcdefghijklmnopqrstuvwxyz",
};

constexpr int32_t CorpusCount = static_cast<int32_t>(sizeof(ChurnCorpus) / sizeof(ChurnCorpus[0]));

int64_t RunsForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 200;
    case Tier::M:
      return 1000;
    case Tier::L:
      return 4000;
    case Tier::XL:
      return 16000;
  }
  return 1000;
}

class GlyphChurnScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "res.glyph_churn";
      result.category = "res";
      result.description =
          "Text runs whose size and content change every frame, so the glyph cache keeps missing";
      result.realWorkload = "Rich text editing, animated typography and continuously rescaling "
                            "labels";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCache | BottleneckCPURecord;
      result.params = {{"runs", int64_t{1000}, "text runs per frame", true},
                       {"sizes", int64_t{16}, "distinct font sizes", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("runs", RunsForTier(tier));
    params.setInt("sizes", 16);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto runs = params.getInt("runs");
    auto sizes = params.getInt("sizes");
    _runCount = static_cast<size_t>(runs);

    auto typeface = sceneContext.resources->typeface("font/NotoSansSC-Regular.otf");
    if (typeface == nullptr) {
      return false;
    }
    _fonts.clear();
    _fonts.reserve(static_cast<size_t>(sizes));
    for (int64_t i = 0; i < sizes; i++) {
      // Fractional sizes make every entry its own cache key instead of rounding to a few buckets.
      auto size = (10.0f + static_cast<float>(i) * 3.7f) * _density;
      _fonts.push_back(tgfx::Font(typeface, size));
    }
    _paint = tgfx::Paint();
    _paint.setColor(tgfx::Color{0.12f, 0.13f, 0.17f, 1.0f});
    _paint.setAntiAlias(true);

    Random random(0x6179B001u);
    _runs.clear();
    _runs.reserve(_runCount);
    for (size_t i = 0; i < _runCount; i++) {
      TextRun run = {};
      run.x = random.range(0.0f, _width - 400.0f * _density);
      run.y = random.range(0.0f, _height);
      run.textIndex = static_cast<size_t>(random.rangeInt(0, CorpusCount - 1));
      _runs.push_back(run);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    // Every frame shifts which font each run uses, so the same glyph is requested at a different
    // size and the active font instance keeps changing.
    _fontOffset = static_cast<size_t>(frameIndex * 7);
  }

  void draw(tgfx::Canvas* canvas) override {
    auto fontCount = _fonts.size();
    for (size_t i = 0; i < _runs.size(); i++) {
      const auto& run = _runs[i];
      auto font = _fonts[(_fontOffset + i * 31) % fontCount];
      canvas->drawSimpleText(ChurnCorpus[run.textIndex], run.x, run.y, font, _paint);
    }
  }

  void teardown() override {
    _fonts.clear();
    _runs.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  struct TextRun {
    float x = 0.0f;
    float y = 0.0f;
    size_t textIndex = 0;
  };

  size_t _runCount = 0;
  size_t _fontOffset = 0;
  tgfx::Paint _paint = {};
  std::vector<tgfx::Font> _fonts = {};
  std::vector<TextRun> _runs = {};
};

}  // namespace

BENCH_SCENE(GlyphChurnScene);

}  // namespace benchmark
