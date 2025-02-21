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

struct RectData {
  tgfx::Rect rect{0, 0, 1, 1};
  float speed;
};

class SolidRectBench : public Bench {
 public:
  SolidRectBench() : Bench("SolidRectBench") {
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void Init(const AppHost* host);

  void InitRects();

  void InitPaints();

  void AnimateRects();

  void FlushStatus(const AppHost* host);

  void DrawRects(tgfx::Canvas* canvas) const;

  void DrawStatus(tgfx::Canvas* canvas, const AppHost* host) const;

 private:
  float width = 0;   //appHost width
  float height = 0;  //appHost height
  size_t drawCount = 800;
  std::vector<RectData> rects;
  tgfx::Paint paints[3];  // red, green, blue solid paints
  int64_t lastFlushTime = -1;
  tgfx::Font fpsFont = {};
  tgfx::Color fpsColor = tgfx::Color::Green();
  std::vector<std::string> status = {};
};

}  // namespace benchmark
