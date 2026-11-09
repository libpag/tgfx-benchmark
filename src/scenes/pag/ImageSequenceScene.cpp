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
#include "core/SceneRegistry.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageInfo.h"

namespace benchmark {
namespace {

constexpr int RingSize = 4;

int64_t LayersForTier(Tier tier) {
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

class ImageSequenceScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.image_sequence";
      result.category = "pag";
      result.description =
          "Raw RGBA frames uploaded and drawn every frame, as a decoded image sequence would be";
      result.realWorkload = "Image sequence export and frame by frame video playback";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckUpload | BottleneckBandwidth;
      result.params = {{"layers", int64_t{4}, "number of sequence layers", true},
                       {"frameSize", int64_t{512}, "frame edge in logical pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    params.setInt("frameSize", 512);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    auto layers = params.getInt("layers");
    auto frameSize = static_cast<int>(params.getInt("frameSize"));

    _info = tgfx::ImageInfo::Make(frameSize, frameSize, tgfx::ColorType::RGBA_8888,
                                  tgfx::AlphaType::Premultiplied);
    _buffers.clear();
    _data.clear();
    _frames.clear();

    // A ring per layer: reusing one buffer would let the image cache hand back the previous
    // upload, and the scenario is precisely about uploading a new frame every time.
    auto total = static_cast<size_t>(layers) * RingSize;
    _buffers.resize(total);
    _data.resize(total);
    _frames.resize(static_cast<size_t>(layers));

    auto byteSize = _info.byteSize();
    for (size_t i = 0; i < total; i++) {
      _buffers[i].resize(byteSize);
      for (size_t offset = 0; offset + 3 < byteSize; offset += 4) {
        auto pixel = static_cast<uint32_t>(offset / 4);
        _buffers[i][offset] = static_cast<uint8_t>(pixel % 251);
        _buffers[i][offset + 1] = static_cast<uint8_t>((pixel / 7) % 241);
        _buffers[i][offset + 2] = static_cast<uint8_t>(i * 17);
        _buffers[i][offset + 3] = 255;
      }
      _data[i] = tgfx::Data::MakeWithoutCopy(_buffers[i].data(), _buffers[i].size());
    }

    // Layers are offset slightly so that the composite visibly depends on the layer count
    // instead of every frame stacking into the same picture.
    auto size = static_cast<float>(frameSize);
    for (int64_t i = 0; i < layers; i++) {
      auto offset = static_cast<float>(i % 8) * 12.0f * sceneContext.density;
      _frames[static_cast<size_t>(i)] = tgfx::Rect::MakeXYWH(
          _width * 0.5f - size * 0.5f + offset, _height * 0.5f - size * 0.5f + offset, size,
          size);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    _ringIndex = static_cast<size_t>(frameIndex % RingSize);
  }

  void draw(tgfx::Canvas* canvas) override {
    auto layers = _frames.size();
    for (size_t i = 0; i < layers; i++) {
      auto slot = i * RingSize + _ringIndex;
      auto image = tgfx::Image::MakeFrom(_info, _data[slot]);
      if (image != nullptr) {
        canvas->drawImageRect(image, _frames[i]);
      }
    }
  }

  void teardown() override {
    _buffers.clear();
    _data.clear();
    _frames.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  tgfx::ImageInfo _info = {};
  size_t _ringIndex = 0;
  std::vector<std::vector<uint8_t>> _buffers = {};
  std::vector<std::shared_ptr<tgfx::Data>> _data = {};
  std::vector<tgfx::Rect> _frames = {};
};

}  // namespace

BENCH_SCENE(ImageSequenceScene);

}  // namespace benchmark
