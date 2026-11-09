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
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"

namespace benchmark {
namespace {

const char* const EncodedAssets[] = {
    "images/mandrill_128.png",
    "images/mandrill_128.jpg",
    "images/mandrill_128.webp",
};

constexpr size_t AssetCount = sizeof(EncodedAssets) / sizeof(EncodedAssets[0]);

int64_t DecodesForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 4;
    case Tier::M:
      return 16;
    case Tier::L:
      return 64;
    case Tier::XL:
      return 256;
  }
  return 16;
}

class DecodeScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "res.decode";
      result.category = "res";
      result.description =
          "Encoded jpg, png and webp bytes decoded and uploaded on every frame";
      result.realWorkload = "Scrolling an image gallery where new thumbnails appear continuously";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckUpload | BottleneckBandwidth;
      result.params = {{"decodes", int64_t{16}, "images decoded per frame", true}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("decodes", DecodesForTier(tier));
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    auto decodes = params.getInt("decodes");

    // Each entry owns its own copy of the bytes, so MakeFromEncoded cannot hand back a cached
    // image: the decode and the upload really happen again on every frame.
    _blobs.clear();
    _blobs.reserve(static_cast<size_t>(decodes));
    for (int64_t i = 0; i < decodes; i++) {
      auto source = sceneContext.resources->data(EncodedAssets[static_cast<size_t>(i) % AssetCount]);
      if (source == nullptr) {
        return false;
      }
      auto copy = tgfx::Data::MakeWithCopy(source->data(), source->size());
      if (copy == nullptr) {
        return false;
      }
      _blobs.push_back(copy);
    }

    Random random(0xDEC0DE01u);
    auto minSize = 64.0f * _density;
    auto maxSize = 256.0f * _density;
    _rects.clear();
    _rects.reserve(static_cast<size_t>(decodes));
    for (int64_t i = 0; i < decodes; i++) {
      auto size = random.range(minSize, maxSize);
      _rects.push_back(tgfx::Rect::MakeXYWH(random.range(0.0f, _width - size),
                                            random.range(0.0f, _height - size), size, size));
    }
    return true;
  }

  void update(int64_t) override {
  }

  void draw(tgfx::Canvas* canvas) override {
    for (size_t i = 0; i < _blobs.size(); i++) {
      auto image = tgfx::Image::MakeFromEncoded(_blobs[i]);
      if (image != nullptr) {
        canvas->drawImageRect(image, _rects[i]);
      }
    }
  }

  void teardown() override {
    _blobs.clear();
    _rects.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  float _density = 1.0f;
  std::vector<std::shared_ptr<tgfx::Data>> _blobs = {};
  std::vector<tgfx::Rect> _rects = {};
};

}  // namespace

BENCH_SCENE(DecodeScene);

}  // namespace benchmark
