/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx-benchmark available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <deque>
#include "base/Drawer.h"

namespace benchmark {

struct RectData {
  tgfx::Rect rect{0, 0, 1, 1};
  float speed;
};

class SolidRectBench : public Drawer {
 public:
  SolidRectBench() : Drawer("SolidRectBench") {
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void Init(const AppHost* host);

  void InitRects();

  void InitPaints();

  void DrawRects(tgfx::Canvas* canvas) const;

  void DrawFPS(tgfx::Canvas* canvas, const AppHost* host);

  void AnimateRects();

 private:
  float _width = 1024.f;  //appHost width
  float _height = 720.f;  //appHost height
  size_t _frameCount = 0;
  size_t _curRectCount = 0;
  std::string _fpsInfo;
  std::vector<RectData> _rects;
  bool _initialized = false;
  tgfx::Paint _paints[3];           // red, green, blue solid paints
  tgfx::Paint _fpsBackgroundpaint;  // red solid paint
  tgfx::Paint _fpsTextPaint;        // white solid paint
  uint64_t _lastMs = 0;
  std::deque<uint64_t> _timeStamps;
  tgfx::Rect _fpsBackgroundRect;
  tgfx::Font _fpsFont;
};

}  // namespace benchmark
