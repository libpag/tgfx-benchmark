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
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include "core/Random.h"
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Picture.h"
#include "tgfx/core/PictureRecorder.h"
#include "tgfx/core/Shader.h"

namespace benchmark {
namespace {

int64_t ReplaysForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 1;
    case Tier::M:
      return 4;
    case Tier::L:
      return 16;
    case Tier::XL:
      return 64;
  }
  return 4;
}

/// Records a PAG like composition once and replays it. This is how libpag actually renders: the
/// animation is recorded into a Picture and played back every frame.
class PictureScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.picture";
      result.category = "pag";
      result.description =
          "A recorded Picture containing gradients, shapes, an image and text, replayed N times";
      result.realWorkload = "libpag playback, where every frame replays a recorded composition";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCPURecord | BottleneckVertex;
      result.params = {{"replays", int64_t{4}, "picture playbacks per frame", true},
                       {"items", int64_t{40}, "draw commands inside the picture", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("replays", ReplaysForTier(tier));
    params.setInt("items", 40);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto items = params.getInt("items");

    auto typeface = sceneContext.resources->typeface("font/NotoSansSC-Regular.otf");
    if (typeface == nullptr) {
      return false;
    }
    auto font = tgfx::Font(typeface, 18.0f * _density);
    auto image = sceneContext.resources->image("images/mandrill_128.png");

    tgfx::PictureRecorder recorder = {};
    auto* canvas = recorder.beginRecording();
    if (canvas == nullptr) {
      return false;
    }
    Random random(0x1C7E0001u);
    auto tileSize = 120.0f * _density;
    for (int64_t i = 0; i < items; i++) {
      auto x = random.range(0.0f, _width - tileSize);
      auto y = random.range(0.0f, _height - tileSize);
      auto rect = tgfx::Rect::MakeXYWH(x, y, tileSize, tileSize);
      switch (i % 4) {
        case 0: {
          tgfx::Paint paint = {};
          auto colors = std::vector<tgfx::Color>{
              {random.nextFloat(), random.nextFloat(), 0.9f, 1.0f},
              {0.9f, random.nextFloat(), random.nextFloat(), 1.0f}};
          paint.setShader(tgfx::Shader::MakeLinearGradient(
              {x, y}, {x + tileSize, y + tileSize}, colors));
          canvas->drawRect(rect, paint);
          break;
        }
        case 1: {
          tgfx::Paint paint = {};
          paint.setColor(
              tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 0.9f});
          canvas->drawRRect(tgfx::RRect::MakeRectXY(rect, 12.0f, 12.0f), paint);
          break;
        }
        case 2: {
          if (image != nullptr) {
            canvas->drawImageRect(image, rect);
          }
          break;
        }
        default: {
          tgfx::Paint paint = {};
          paint.setColor(tgfx::Color{0.1f, 0.12f, 0.18f, 1.0f});
          canvas->drawSimpleText("REC", x + 8.0f, y + 24.0f, font, paint);
          break;
        }
      }
    }
    _picture = recorder.finishRecordingAsPicture();
    if (_picture == nullptr) {
      return false;
    }
    _replays = params.getInt("replays");
    return true;
  }

  void update(int64_t frameIndex) override {
    _offset = static_cast<float>(frameIndex % 60) * 0.5f;
  }

  void draw(tgfx::Canvas* canvas) override {
    for (int64_t i = 0; i < _replays; i++) {
      canvas->save();
      // A small per replay shift keeps the copies from stacking into one identical image.
      canvas->translate(static_cast<float>(i % 8) * 6.0f + _offset,
                        static_cast<float>(i / 8) * 6.0f);
      canvas->drawPicture(_picture);
      canvas->restore();
    }
  }

  void teardown() override {
    _picture = nullptr;
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  float _offset = 0.0f;
  int64_t _replays = 0;
  std::shared_ptr<tgfx::Picture> _picture = {};
};

}  // namespace

BENCH_SCENE(PictureScene);

}  // namespace benchmark
