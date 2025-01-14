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

#include "SolidRectBench.h"
#include <iomanip>
#include <random>
#include <sstream>
#include "tgfx/layers/ShapeLayer.h"

namespace benchmark {
static constexpr size_t kMaxRectCount = 80000;
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
  rects.resize(kMaxRectCount);
  std::mt19937 rectRng(18);
  std::mt19937 speedRng(36);
  std::uniform_real_distribution<float> rectDistribution(0, 1);
  std::uniform_real_distribution<float> speedDistribution(1, 2);
  for (size_t i = 0; i < kMaxRectCount; i++) {
    const auto x = rectDistribution(rectRng) * width;
    const auto y = rectDistribution(rectRng) * height;
    const auto size = 10.f + rectDistribution(rectRng) * 40.f;
    rects[i].rect.setXYWH(x, y, size, size);
    rects[i].speed = speedDistribution(speedRng);
  }
}

void SolidRectBench::InitPaints() {
  for (auto i = 0; i < 3; i++) {
    tgfx::Color color = tgfx::Color::Black();
    color[i] = 1.f;
    paints[i].setColor(color);
    paints[i].setAntiAlias(false);
  }
  fpsBackgroundpaint.setColor(tgfx::Color{0.32f, 0.42f, 0.62f, 0.9f});
}

void SolidRectBench::Init(const AppHost* host) {
  if (initialized) {
    return;
  }
  width = static_cast<float>(host->width());
  height = static_cast<float>(host->height());
  fpsBackgroundRect = tgfx::Rect::MakeWH(width * 1.f, kFpsBackgroundHeight * host->density());
  fpsFont = tgfx::Font(host->getTypeface("default"), kFontSize * host->density());
  InitRects();
  InitPaints();
  initialized = true;
}

void SolidRectBench::AnimateRects() {
  for (size_t i = 0; i < curRectCount; i++) {
    auto& rect = rects[i].rect;
    rect.offset(-rects[i].speed, 0);
    if (rect.right < 0) {
      rect.offset(width - rect.left, 0);
    }
  }
}

void SolidRectBench::DrawRects(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < curRectCount; i++) {
    canvas->drawRect(rects[i].rect, paints[i % 3]);
  }
}

void SolidRectBench::DrawFPS(tgfx::Canvas* canvas, const AppHost* host) {
  frameCount++;
  if (lastMs == 0) {
    lastMs = GetCurrentTimeStampInMs();
    fpsTextPaint.setColor(tgfx::Color::Green());
  }

  auto currentMs = GetCurrentTimeStampInMs();
  if (!timeStamps.empty()) {
    while (currentMs - timeStamps.front() > 1000) {
      timeStamps.pop_front();
      if (timeStamps.empty()) {
        break;
      }
    }

    auto timeInterval = currentMs - lastMs;
    if (!timeStamps.empty() && timeInterval > kFpsFlushInterval) {
      auto fps = 1000.f * timeStamps.size() / (currentMs - timeStamps.front());
      std::stringstream ss;
      ss << "Rectangles: " << curRectCount << ", FPS:" << std::fixed << std::setprecision(1) << fps;
      fpsInfo = ss.str();
      auto accCount =
          static_cast<size_t>(1.f * timeInterval / kFpsFlushInterval * kRectIncreaseStep);
      curRectCount = std::min(curRectCount + accCount, kMaxRectCount);
      lastMs = currentMs;
      frameCount = 0;
      auto fpsColor = tgfx::Color::Green();
      if (fps > 58.f) {
        fpsColor = tgfx::Color::Green();
      } else if (fps > 30.f) {
        fpsColor = tgfx::Color{1.f, 1.f, 0.f, 1.f};  //Yellow
      } else {
        fpsColor = tgfx::Color{0.91f, 0.31f, 0.28f, 1.f};
      }
      fpsTextPaint.setColor(fpsColor);
    }
  }
  timeStamps.push_back(currentMs);

  canvas->drawRect(fpsBackgroundRect, fpsBackgroundpaint);
  canvas->drawSimpleText(fpsInfo, kFpsTextMarginLeft * host->density(),
                         kFpsTextMarginTop * host->density(), fpsFont, fpsTextPaint);
}

void SolidRectBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  Init(host);
  DrawRects(canvas);
  DrawFPS(canvas, host);
  AnimateRects();
}

}  // namespace benchmark
