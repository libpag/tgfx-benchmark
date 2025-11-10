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

namespace benchmark {

enum class GraphicType { Rect, Circle, Oval, RRect, Star };

struct GraphicData {
  tgfx::Rect rect{0, 0, 1, 1};
  float speedX;
  float speedY;
};

struct DrawParam {
  size_t startCount = 1;
  size_t stepCount = 600;
  float minFPS = 60.0f;
  size_t maxCount = 1000000;
};

struct PerfData {
  float fps = 0.0f;
  float drawTime = 0.0f;
  size_t drawCount = 0;
};

class ParticleBench : public Bench {
 public:
  ParticleBench() : Bench("ParticleBench") {
  }

  explicit ParticleBench(GraphicType type);

  static void ShowPerfData(bool status);

  static void SetInitDrawCount(size_t count);

  static void SetMaxDrawCount(size_t count);

  static void SetStepDrawCount(size_t count);

  static void SetTargetFPS(float fps);

  static void SetAntiAlias(bool aa);

  static void SetStroke(bool stroke);

  static void SetLineJoinType(int type);

  bool isMaxDrawCountReached() const;

  PerfData getPerfData() const;

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

  void DrawStar(tgfx::Canvas* canvas) const;

  void DrawGraphics(tgfx::Canvas* canvas) const;

 private:
  float width = 0;   //appHost width
  float height = 0;  //appHost height
  size_t drawCount = 1;
  float currentFPS = 0.f;
  std::vector<GraphicData> graphics = {};
  std::vector<tgfx::Path> paths = {};
  tgfx::Rect startRect = tgfx::Rect::MakeEmpty();
  tgfx::Paint paints[3];  // red, green, blue solid paints
  int64_t lastFlushTime = -1;
  tgfx::Font fpsFont = {};
  tgfx::Color fpsColor = tgfx::Color::Green();
  std::vector<std::string> status = {};
  GraphicType graphicType = GraphicType::Rect;
  bool maxDrawCountReached = false;
  PerfData perfData = {};
};

}  // namespace benchmark