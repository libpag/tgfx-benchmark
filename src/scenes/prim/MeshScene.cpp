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
#include "tgfx/core/Mesh.h"

namespace benchmark {
namespace {

int64_t CopiesForTier(Tier tier) {
  switch (tier) {
    case Tier::S:
      return 64;
    case Tier::M:
      return 600;
    case Tier::L:
      return 2000;
    case Tier::XL:
      return 6000;
  }
  return 600;
}

class MeshScene : public Scene {
 public:
  const SceneInfo& info() const override {
    static const SceneInfo sceneInfo = [] {
      SceneInfo result = {};
      result.id = "prim.mesh";
      result.category = "prim";
      result.description =
          "A single indexed triangle mesh with per vertex colors covering the viewport";
      result.realWorkload = "Warped images, 3D transforms and custom geometry";
      result.modules = ModuleCore;
      result.bottlenecks = BottleneckVertex | BottleneckFillRate;
      result.params = {{"copies", int64_t{16}, "mesh instances per frame", true},
                       {"cells", int64_t{64}, "grid resolution along one axis", false}};
      return result;
    }();
    return sceneInfo;
  }

  ParamSet paramsForTier(Tier tier) const override {
    ParamSet params;
    params.setInt("copies", CopiesForTier(tier));
    params.setInt("cells", 64);
    return params;
  }

  bool setup(const SceneContext& sceneContext, const ParamSet& params) override {
    _width = static_cast<float>(sceneContext.width);
    _height = static_cast<float>(sceneContext.height);
    _copies = static_cast<int>(params.getInt("copies"));
    if (_copies < 1) {
      _copies = 1;
    }
    auto cells = static_cast<int>(params.getInt("cells"));
    if (cells < 1) {
      cells = 1;
    }

    Random random(0x3E5A0001u);
    const auto stride = cells + 1;
    std::vector<tgfx::Point> positions;
    std::vector<tgfx::Color> colors;
    positions.reserve(static_cast<size_t>(stride) * static_cast<size_t>(stride));
    colors.reserve(positions.capacity());

    // The mesh covers a fixed area so that raising the cell count adds vertices without adding
    // fill: otherwise the scene is fill bound and the vertex axis does not move the timer.
    const auto areaWidth = 640.0f * sceneContext.density;
    const auto areaHeight = 640.0f * sceneContext.density;
    const auto originX = (_width - areaWidth) * 0.5f;
    const auto originY = (_height - areaHeight) * 0.5f;
    auto cellWidth = areaWidth / static_cast<float>(cells);
    auto cellHeight = areaHeight / static_cast<float>(cells);
    for (auto y = 0; y <= cells; y++) {
      for (auto x = 0; x <= cells; x++) {
        positions.push_back(tgfx::Point{originX + static_cast<float>(x) * cellWidth,
                                        originY + static_cast<float>(y) * cellHeight});
        colors.push_back(tgfx::Color{random.range(0.3f, 1.0f), random.range(0.3f, 1.0f),
                                     random.range(0.3f, 1.0f), 1.0f});
      }
    }

    std::vector<uint16_t> indices;
    indices.reserve(static_cast<size_t>(cells) * static_cast<size_t>(cells) * 6U);
    for (auto y = 0; y < cells; y++) {
      for (auto x = 0; x < cells; x++) {
        auto topLeft = static_cast<uint16_t>(y * stride + x);
        auto topRight = static_cast<uint16_t>(topLeft + 1);
        auto bottomLeft = static_cast<uint16_t>(topLeft + stride);
        auto bottomRight = static_cast<uint16_t>(bottomLeft + 1);
        indices.push_back(topLeft);
        indices.push_back(topRight);
        indices.push_back(bottomLeft);
        indices.push_back(topRight);
        indices.push_back(bottomRight);
        indices.push_back(bottomLeft);
      }
    }

    _mesh = tgfx::Mesh::MakeCopy(tgfx::MeshTopology::Triangles, static_cast<int>(positions.size()),
                                 positions.data(), nullptr, colors.data(),
                                 static_cast<int>(indices.size()), indices.data());
    if (_mesh == nullptr) {
      return false;
    }
    _paint = tgfx::Paint();
    _paint.setAntiAlias(false);
    return true;
  }

  void update(int64_t frameIndex) override {
    _shift = static_cast<float>(frameIndex % 120) * 0.25f;
  }

  void draw(tgfx::Canvas* canvas) override {
    // A single mesh is far too cheap on modern GPUs, so the vertex load comes from drawing the
    // same indexed mesh many times with small offsets.
    for (int i = 0; i < _copies; i++) {
      auto offset = static_cast<float>(i) * 6.0f;
      canvas->save();
      canvas->concat(tgfx::Matrix::MakeTrans(offset + _shift, offset * 0.5f));
      canvas->drawMesh(_mesh, _paint);
      canvas->restore();
    }
  }

  void teardown() override {
    _mesh = nullptr;
  }

 private:
  float _width = 0.0f;
  float _height = 0.0f;
  int _copies = 1;
  float _shift = 0.0f;
  tgfx::Paint _paint = {};
  std::shared_ptr<tgfx::Mesh> _mesh = {};
};

}  // namespace

BENCH_SCENE(MeshScene);

}  // namespace benchmark
