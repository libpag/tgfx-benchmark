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
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageInfo.h"

namespace benchmark {
namespace {

int64_t ImagesForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 8;
    case Tier::M:
      return 32;
    case Tier::L:
      return 128;
    case Tier::XL:
      return 512;
  }
  return 32;
}

class CachePressureScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "res.cache_pressure";
      result.category = "res";
      result.description =
          "Many distinct textures drawn every frame with a deliberately small GPU cache limit";
      result.realWorkload = "Memory constrained devices cycling through more assets than fit in "
                            "the resource cache";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckCache | BottleneckUpload;
      result.params = {{"images", int64_t{32}, "distinct textures", true},
                       {"cacheLimitMB", int64_t{16}, "GPU cache limit in megabytes", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("images", ImagesForTier(tier));
    params.setInt("cacheLimitMB", 16);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto images = params.getInt("images");
    auto cacheLimitMB = params.getInt("cacheLimitMB");
    if (sceneContext.context != nullptr) {
      // Restored in teardown: the limit is process wide and would otherwise leak into whatever
      // scene runs next in the same process.
      _previousCacheLimit = sceneContext.context->cacheLimit();
      sceneContext.context->setCacheLimit(static_cast<size_t>(cacheLimitMB) * 1024U * 1024U);
      _context = sceneContext.context;
    }

    Random random(0xCA9E0001u);
    _images.clear();
    _rects.clear();
    _images.reserve(static_cast<size_t>(images));
    _rects.reserve(static_cast<size_t>(images));

    const auto edge = 256;
    auto info = tgfx::ImageInfo::Make(edge, edge, tgfx::ColorType::RGBA_8888,
                                      tgfx::AlphaType::Premultiplied);
    std::vector<uint8_t> pixels(info.byteSize());
    for (int64_t i = 0; i < images; i++) {
      // Each texture has distinct pixels so the cache cannot deduplicate them.
      auto tint = static_cast<uint8_t>(i * 37);
      for (auto y = 0; y < edge; y++) {
        for (auto x = 0; x < edge; x++) {
          auto offset = (static_cast<size_t>(y) * edge + static_cast<size_t>(x)) * 4U;
          pixels[offset] = static_cast<uint8_t>(tint + x);
          pixels[offset + 1] = static_cast<uint8_t>(tint + y);
          pixels[offset + 2] = tint;
          pixels[offset + 3] = 255;
        }
      }
      auto data = tgfx::Data::MakeWithCopy(pixels.data(), pixels.size());
      auto image = tgfx::Image::MakeFrom(info, data);
      if (image == nullptr) {
        return false;
      }
      _images.push_back(image);

      auto size = random.range(96.0f, 320.0f) * _density;
      _rects.push_back(tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                            random.range(0.0f, _height - size), size, size));
    }
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    for (size_t i = 0; i < _images.size(); i++) {
      canvas->drawImageRect(_images[i], _rects[i]);
    }
  }

  void teardown() override {
    _images.clear();
    _rects.clear();
    if (_context != nullptr) {
      _context->setCacheLimit(_previousCacheLimit);
      _context = nullptr;
    }
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  tgfx::Context* _context = nullptr;
  size_t _previousCacheLimit = 0;
  std::vector<std::shared_ptr<tgfx::Image>> _images = {};
  std::vector<tgfx::Rect> _rects = {};
};

}  // namespace

BENCH_SCENE(CachePressureScene);

}  // namespace benchmark
