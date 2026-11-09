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

#include <cmath>
#include <memory>
#include <string>
#include "core/ResourceStore.h"
#include "core/SceneRegistry.h"
#include "tgfx/core/Stream.h"
#include "tgfx/svg/SVGDOM.h"

namespace benchmark {
namespace {

int64_t CopiesForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 16;
    case Tier::M:
      return 64;
    case Tier::L:
      return 256;
    case Tier::XL:
      return 1024;
  }
  return 64;
}

class SvgZoomScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "vec.svg_zoom";
      result.category = "vec";
      result.description =
          "An SVG document scaled continuously from 0.25x to 4x, forcing re-rasterization at "
          "every zoom step";
      result.realWorkload = "Zooming a vector illustration or an icon preview";
      result.modules = ModuleSVG;
      result.bottlenecks = BottleneckCPURecord | BottleneckCache;
      result.params = {{"copies", int64_t{2}, "SVG instances per frame", true},
                       {"file", std::string("svg/complex1.svg"), "svg resource path", false}};
      result.checkpointFrames = {0, 40};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("copies", CopiesForTier(tier));
    params.setString("file", "svg/complex1.svg");
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _density = sceneContext.density;
    _copies = params.getInt("copies");

    auto stream = tgfx::Stream::MakeFromFile(
        sceneContext.resources->path(params.getString("file")));
    if (stream == nullptr) {
      return false;
    }
    _svgDom = tgfx::SVGDOM::Make(*stream);
    if (_svgDom == nullptr) {
      return false;
    }
    _cellSize = _width / 8.0f;
    _documentSize = _cellSize * 0.8f;
    _svgDom->setContainerSize(tgfx::Size{_documentSize, _documentSize});
    return true;
  }

  void update(int64_t frameIndex) override {
    // Triangle wave in log space so every zoom level gets the same number of frames.
    const auto period = 80.0;
    auto phase = static_cast<double>(frameIndex % static_cast<int64_t>(period)) / period;
    auto triangle = phase < 0.5 ? phase * 2.0 : 2.0 - phase * 2.0;
    const auto minZoom = std::log(0.25);
    const auto maxZoom = std::log(2.0);
    _zoom = static_cast<float>(std::exp(minZoom + triangle * (maxZoom - minZoom)));
  }

  void draw(tgfx::Canvas* canvas) override {
    const auto columns = 8;
    for (int64_t i = 0; i < _copies; i++) {
      auto column = i % columns;
      auto row = i / columns;
      // Off screen copies are still submitted: the axis has to keep adding work.
      auto cellY = _cellSize * static_cast<float>(row);
      canvas->save();
      canvas->translate(_cellSize * static_cast<float>(column) + _cellSize * 0.5f,
                        cellY + _cellSize * 0.5f);
      canvas->scale(_zoom, _zoom);
      canvas->translate(-_documentSize * 0.5f, -_documentSize * 0.5f);
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
  float _zoom = 1.0f;
  float _documentSize = 0.0f;
  float _cellSize = 0.0f;
  int64_t _copies = 0;
  std::shared_ptr<tgfx::SVGDOM> _svgDom = {};
};

}  // namespace

BENCH_SCENE(SvgZoomScene);

}  // namespace benchmark
