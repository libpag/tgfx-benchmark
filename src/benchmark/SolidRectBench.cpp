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

#include "SolidRectBench.h"
#include <iomanip>
#include <random>
#include <sstream>
#include "tgfx/layers/ShapeLayer.h"

namespace benchmark {
static constexpr size_t kMaxRectCount = 50000;
constexpr float kFpsBackgroundHeight = 48.f;
constexpr float kFontSize = 40.f;
constexpr float kFpsTextMarginLeft = 262.f;
constexpr float kFpsTextMarginTop = 38.f;
constexpr uint64_t kFpsFlushInterval = 333;
constexpr int kRectIncreaseStep = 400;

uint64_t GetCurrentTimeStampInMs() {
  auto now = std::chrono::steady_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  return static_cast<uint64_t>(time.count());
}

void SolidRectBench::InitRects() {
  _rects.resize(kMaxRectCount);
  std::mt19937 rectRng(18);
  std::mt19937 speedRng(36);
  std::uniform_real_distribution<float> rectDistribution(0, 1);
  std::uniform_real_distribution<float> speedDistribution(1, 2);
  for (size_t i = 0; i < kMaxRectCount; i++) {
    const auto x = rectDistribution(rectRng) * _width;
    const auto y = rectDistribution(rectRng) * _height;
    const auto width = 10.f + rectDistribution(rectRng) * 40.f;
    _rects[i].rect.setXYWH(x, y, width, width);
    _rects[i].speed = speedDistribution(speedRng);
  }
}

void SolidRectBench::InitPaints() {
  for (auto i = 0; i < 3; i++) {
    tgfx::Color color = tgfx::Color::Black();
    color[i] = 1.f;
    _paints[i].setColor(color);
    _paints[i].setAntiAlias(false);
  }
  _fpsBackgroundpaint.setColor(tgfx::Color{0.32f, 0.42f, 0.62f, 0.9f});
}

void SolidRectBench::Init(const AppHost* host) {
  if (_initialized) {
    return;
  }
  _width = static_cast<float>(host->width());
  _height = static_cast<float>(host->height());
  _fpsBackgroundRect = tgfx::Rect::MakeWH(_width * 1.f, kFpsBackgroundHeight * host->density());
  _fpsFont = tgfx::Font(host->getTypeface("default"), kFontSize * host->density());
  InitRects();
  InitPaints();
  _initialized = true;
}

void SolidRectBench::AnimateRects() {
  for (size_t i = 0; i < _curRectCount; i++) {
    auto& rect = _rects[i].rect;
    rect.offset(-_rects[i].speed, 0);
    if (rect.right < 0) {
      rect.offset(_width - rect.left, 0);
    }
  }
}

void SolidRectBench::DrawRects(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < _curRectCount; i++) {
    canvas->drawRect(_rects[i].rect, _paints[i % 3]);
  }
}

void SolidRectBench::DrawFPS(tgfx::Canvas* canvas, const AppHost* host) {
  _frameCount++;
  if (_lastMs == 0) {
    _lastMs = GetCurrentTimeStampInMs();
    _fpsTextPaint.setColor(tgfx::Color::Green());
  }

  auto currentMs = GetCurrentTimeStampInMs();
  if (!_timeStamps.empty()) {
    while (currentMs - _timeStamps.front() > 1000) {
      _timeStamps.pop_front();
      if (_timeStamps.empty()) {
        break;
      }
    }

    auto timeInterval = currentMs - _lastMs;
    if (!_timeStamps.empty() && timeInterval > kFpsFlushInterval) {
      auto fps = 1000.f * _timeStamps.size() / (currentMs - _timeStamps.front());
      std::stringstream ss;
      ss << "Rectangles: " << _curRectCount << ", FPS:" << std::fixed << std::setprecision(1)
         << fps;
      _fpsInfo = ss.str();
      auto accCount =
          static_cast<size_t>(1.f * timeInterval / kFpsFlushInterval * kRectIncreaseStep);
      _curRectCount = std::min(_curRectCount + accCount, kMaxRectCount);
      _lastMs = currentMs;
      _frameCount = 0;
      auto fpsColor = tgfx::Color::Green();
      if (fps > 58.f) {
        fpsColor = tgfx::Color::Green();
      } else if (fps > 30.f) {
        fpsColor = tgfx::Color{1.f, 1.f, 0.f, 1.f};  //Yellow
      } else {
        fpsColor = tgfx::Color{0.91f, 0.31f, 0.28f, 1.f};
      }
      _fpsTextPaint.setColor(fpsColor);
    }
  }
  _timeStamps.push_back(currentMs);

  canvas->drawRect(_fpsBackgroundRect, _fpsBackgroundpaint);
  canvas->drawSimpleText(_fpsInfo, kFpsTextMarginLeft * host->density(),
                         kFpsTextMarginTop * host->density(), _fpsFont, _fpsTextPaint);
}

void SolidRectBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  Init(host);
  DrawRects(canvas);
  DrawFPS(canvas, host);
  AnimateRects();
}

}  // namespace benchmark
