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
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include "core/SceneRegistry.h"
#include "tgfx/core/YUVData.h"

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

class VideoYUVScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "pag.video_yuv";
      result.category = "pag";
      result.description =
          "I420 planes wrapped as YUV images and drawn every frame with BT.709 colour conversion";
      result.realWorkload = "Video layers in motion graphics templates";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckUpload | BottleneckBandwidth;
      result.params = {{"layers", int64_t{4}, "number of video layers", true},
                       {"resolution", int64_t{720}, "frame height in pixels", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("layers", LayersForTier(tier));
    params.setInt("resolution", 720);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    auto layers = params.getInt("layers");
    auto height = static_cast<int>(params.getInt("resolution"));
    auto width = (height * 16) / 9;
    width = (width + 1) & ~1;

    _frameWidth = width;
    _frameHeight = height;
    _yRowBytes = static_cast<size_t>(width);
    _uvRowBytes = static_cast<size_t>(width) / 2U;

    auto total = static_cast<size_t>(layers) * RingSize;
    _yPlanes.resize(total);
    _uvPlanes.resize(total);
    _frames.resize(static_cast<size_t>(layers));

    auto ySize = static_cast<size_t>(width) * static_cast<size_t>(height);
    auto uvSize = ySize / 4U;
    for (size_t i = 0; i < total; i++) {
      _yPlanes[i].resize(ySize);
      _uvPlanes[i].resize(uvSize * 2U);
      for (size_t offset = 0; offset < ySize; offset++) {
        _yPlanes[i][offset] = static_cast<uint8_t>((offset + i * 13U) % 235U + 16U);
      }
      for (size_t offset = 0; offset < uvSize * 2U; offset++) {
        _uvPlanes[i][offset] = static_cast<uint8_t>(128U + (offset + i * 7U) % 32U);
      }
    }

    auto drawWidth = _width * 0.6f;
    auto drawHeight = drawWidth * static_cast<float>(height) / static_cast<float>(width);
    for (size_t i = 0; i < _frames.size(); i++) {
      _frames[i] = tgfx::Rect::MakeXYWH((_width - drawWidth) * 0.5f,
                                        (_height - drawHeight) * 0.5f, drawWidth, drawHeight);
    }
    return true;
  }

  void update(int64_t frameIndex) override {
    _ringIndex = static_cast<size_t>(frameIndex % RingSize);
  }

  void draw(tgfx::Canvas* canvas) override {
    for (size_t i = 0; i < _frames.size(); i++) {
      auto slot = i * RingSize + _ringIndex;
      const void* planes[3] = {
          _yPlanes[slot].data(),
          _uvPlanes[slot].data(),
          _uvPlanes[slot].data() + _uvPlanes[slot].size() / 2U,
      };
      const size_t rowBytes[3] = {_yRowBytes, _uvRowBytes, _uvRowBytes};
      auto yuv = tgfx::YUVData::MakeFrom(_frameWidth, _frameHeight, planes, rowBytes, 3);
      if (yuv == nullptr) {
        continue;
      }
      auto image = tgfx::Image::MakeI420(yuv, tgfx::YUVColorSpace::BT709_LIMITED);
      if (image != nullptr) {
        canvas->drawImageRect(image, _frames[i]);
      }
    }
  }

  void teardown() override {
    _yPlanes.clear();
    _uvPlanes.clear();
    _frames.clear();
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  int _frameWidth = 0;
  int _frameHeight = 0;
  size_t _yRowBytes = 0;
  size_t _uvRowBytes = 0;
  size_t _ringIndex = 0;
  std::vector<std::vector<uint8_t>> _yPlanes = {};
  std::vector<std::vector<uint8_t>> _uvPlanes = {};
  std::vector<tgfx::Rect> _frames = {};
};

}  // namespace

BENCH_SCENE(VideoYUVScene);

}  // namespace benchmark
