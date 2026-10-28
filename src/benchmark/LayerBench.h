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

#pragma once

#include "../base/Bench.h"
#include "tgfx/core/Font.h"

namespace tgfx {
class Layer;
class SolidLayer;
}  // namespace tgfx

namespace benchmark {

enum class LayerBenchType { DropShadow, InnerShadow };

struct ParticleData {
  float size;
  float speedX;
  float speedY;
};

class LayerBench : public Bench {
 public:
  explicit LayerBench(LayerBenchType type);

  static void ShowPerfData(bool status);

  static void SetInitDrawCount(size_t count);

  static void SetMaxDrawCount(size_t count);

  static void SetStepDrawCount(size_t count);

  static void SetTargetFPS(float fps);

  bool isMaxDrawCountReached() const override;

  PerfData getPerfData() const override;

  size_t currentDrawCount() const override {
    return drawCount;
  }

  float currentFPS() const override {
    return perfData.fps;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void Init(const AppHost* host);

  void AnimateParticles(const AppHost* host);

  void SyncParticles(const AppHost* host);

  std::shared_ptr<tgfx::SolidLayer> CreateParticle(size_t index) const;

  void DrawStatus(tgfx::Canvas* canvas, const AppHost* host);

 private:
  float width = 0;   //appHost width
  float height = 0;  //appHost height
  size_t drawCount = 1;
  float _currentFPS = 0.f;
  float density = 1.f;
  std::vector<ParticleData> particleData = {};
  std::vector<std::shared_ptr<tgfx::SolidLayer>> particles = {};
  std::shared_ptr<tgfx::Layer> rootLayer = nullptr;
  int64_t lastFlushTime = -1;
  tgfx::Font fpsFont = {};
  tgfx::Font nameFont = {};
  tgfx::Color fpsColor = tgfx::Color::Green();
  std::vector<std::string> status = {};
  LayerBenchType benchType = LayerBenchType::DropShadow;
  bool maxDrawCountReached = false;
  PerfData perfData = {};
};

}  // namespace benchmark
