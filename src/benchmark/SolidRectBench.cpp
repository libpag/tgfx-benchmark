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
#include "tgfx/core/Clock.h"

namespace benchmark {
static constexpr size_t MAX_RECT_COUNT = 1000000;
static constexpr size_t INCREASE_STEP = 5000;
static constexpr int64_t FLUSH_INTERVAL = 300000;
static constexpr int64_t DRAW_INTERVAL = 16000;
static constexpr float FPS_BACKGROUND_HEIGHT = 50.f;
static constexpr float STATUS_WIDTH = 250.f;
static constexpr float FONT_SIZE = 40.f;

int64_t GetCurrentTimeStampInMs() {
  auto now = std::chrono::steady_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  return static_cast<int64_t>(time.count());
}

void SolidRectBench::InitRects() {
  rects.resize(MAX_RECT_COUNT);
  std::mt19937 rectRng(18);
  std::mt19937 speedRng(36);
  std::uniform_real_distribution<float> rectDistribution(0, 1);
  std::uniform_real_distribution<float> speedDistribution(1, 2);
  for (size_t i = 0; i < MAX_RECT_COUNT; i++) {
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
}

void SolidRectBench::Init(const AppHost* host) {
  auto hostWidth = static_cast<float>(host->width());
  auto hostHeight = static_cast<float>(host->height());
  if (width == hostWidth && height == hostHeight) {
    return;
  }
  status = {};
  width = hostWidth;
  height = hostHeight;
  fpsFont = tgfx::Font(host->getTypeface("default"), FONT_SIZE * host->density());
  InitRects();
  InitPaints();
}

void SolidRectBench::AnimateRects() {
  for (size_t i = 0; i < drawCount; i++) {
    auto& rect = rects[i].rect;
    rect.offset(-rects[i].speed, 0);
    if (rect.right < 0) {
      rect.offset(width - rect.left, 0);
    }
  }
}

void SolidRectBench::DrawRects(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < drawCount; i++) {
    canvas->drawRect(rects[i].rect, paints[i % 3]);
  }
}

void SolidRectBench::FlushStatus(const AppHost* host) {
  auto currentTime = tgfx::Clock::Now();
  if (lastFlushTime == -1) {
    lastFlushTime = currentTime;
  }
  auto flushInterval = currentTime - lastFlushTime;
  if (flushInterval > FLUSH_INTERVAL) {
    auto fps = host->getFPS();
    if (fps <= 0.0f) {
      return;
    }
    auto drawTime = host->getAverageDrawTime();
    auto idleTime = DRAW_INTERVAL - drawTime;
    if (idleTime > 0) {
      auto estimatedCount = static_cast<int64_t>(drawCount) * DRAW_INTERVAL / drawTime;
      drawCount = std::min(drawCount + INCREASE_STEP, static_cast<size_t>(estimatedCount));
      if (drawCount > MAX_RECT_COUNT) {
        drawCount = MAX_RECT_COUNT;
      }
    }
    status.clear();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << fps;
    status.push_back("FPS: " + oss.str());
    oss.str("");
    oss << std::fixed << std::setprecision(1) << static_cast<float>(drawTime) / 1000.f;
    status.push_back("Time: " + oss.str());
    oss.str("");
    oss << drawCount;
    status.push_back("Rects: " + oss.str());
    if (fps > 58.f) {
      fpsColor = tgfx::Color::Green();
    } else if (fps > 30.f) {
      fpsColor = tgfx::Color{1.f, 1.f, 0.f, 1.f};
    } else {
      fpsColor = tgfx::Color{0.91f, 0.31f, 0.28f, 1.f};
    }
    lastFlushTime = currentTime - (flushInterval % FLUSH_INTERVAL);
  }
}

void SolidRectBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  Init(host);
  FlushStatus(host);
  AnimateRects();
  DrawRects(canvas);
  DrawStatus(canvas, host);
}

void SolidRectBench::DrawStatus(tgfx::Canvas* canvas, const AppHost* host) const {
  if (status.empty()) {
    return;
  }
  tgfx::Paint paint = {};
  paint.setColor(tgfx::Color{0.32f, 0.42f, 0.62f, 0.9f});
  auto backgroundRect =
      tgfx::Rect::MakeWH(static_cast<float>(width), FPS_BACKGROUND_HEIGHT * host->density());
  canvas->drawRect(backgroundRect, paint);
  auto top = FONT_SIZE * host->density();
  paint.setColor(fpsColor);
  float left = STATUS_WIDTH * host->density() / 2;
  for (auto& line : status) {
    canvas->drawSimpleText(line, left, top, fpsFont, paint);
    left += STATUS_WIDTH * host->density();
  }
}

}  // namespace benchmark
