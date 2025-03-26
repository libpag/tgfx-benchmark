/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

namespace benchmark {

enum class DataType {
  StartCount = 0,
  StepCount = 1,
  MaxDrawCount = 2,
  MinFPS = 3,
};

struct PerfData {
  float fps = 0.0f;
  float drawTime = 0.0f;
  size_t drawCount = 0;
};

struct RectData {
  tgfx::Rect rect{0, 0, 1, 1};
  float speedX;
  float speedY;
};

class ParticleBench : public Bench {
 public:
  ParticleBench() : Bench("ParticleBench") {
  }

  explicit ParticleBench(GraphicType type)
      : Bench("ParticleBench-" + std::to_string(static_cast<int>(type))), graphicType(type) {
  }

  static void setDrawStatusFlag(bool status);

  static void setDrawParam(int type, float param);

  static bool getMaxDrawCountReached();

  static PerfData getPerfData();

  static void clearPerfData();

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void Init(const AppHost* host);

  void AnimateRects(const AppHost* host);

  void DrawRects(tgfx::Canvas* canvas) const;

  void DrawStatus(tgfx::Canvas* canvas, const AppHost* host);

  void DrawCircle(tgfx::Canvas* canvas) const;

  void DrawRRect(tgfx::Canvas* canvas) const;

  void DrawOval(tgfx::Canvas* canvas) const;

  void DrawGraphics(tgfx::Canvas* canvas) const;

 private:
  float width = 0;   //appHost width
  float height = 0;  //appHost height
  size_t drawCount = 1;
  float currentFPS = 0.f;
  std::vector<RectData> rects = {};
  tgfx::Rect startRect = tgfx::Rect::MakeEmpty();
  tgfx::Paint paints[3];  // red, green, blue solid paints
  int64_t lastFlushTime = -1;
  tgfx::Font fpsFont = {};
  tgfx::Color fpsColor = tgfx::Color::Green();
  std::vector<std::string> status = {};
  GraphicType graphicType = GraphicType::Rect;

  inline static bool drawStatusFlag = true;
  inline static size_t updateDrawCount = 0;
  inline static float targetFPS = 60.0f;
  inline static size_t maxDrawCount = 1000000;
  inline static size_t increaseStep = 600;
  inline static bool maxDrawCountReached = false;

  inline static PerfData perfData = {};
};

}  // namespace benchmark