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

#include "ParticleBench.h"
#include <iomanip>
#include <random>
#include <sstream>
#include "tgfx/core/Clock.h"

namespace benchmark {
static  size_t MAX_RECT_COUNT = 1000000;
static  size_t INCREASE_STEP = 600;
static constexpr int64_t FLUSH_INTERVAL = 300000;
static constexpr float FPS_BACKGROUND_HEIGHT = 50.f;
static constexpr float STATUS_WIDTH = 250.f;
static constexpr float FONT_SIZE = 40.f;
constexpr float PI = 3.14159265358979323846f;


void ParticleBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  Init(host);
  AnimateRects(host);
  if (!host->isWeb()) {
    DrawRects(canvas);
    DrawStatus(canvas, host);
  }else {
    DrawGraphics(canvas, host);
  }
}

void ParticleBench::Init(const AppHost* host) {
  if (host->getUpdateDrawParamFlag()) {
    MAX_RECT_COUNT=host->getMaxDrawCount();
    INCREASE_STEP=host->getStepCount();
    targetFPS = host->getMinFPS();
    host->setUpdateDrawParamFlag(false);
  }
  auto hostWidth = static_cast<float>(host->width());
  auto hostHeight = static_cast<float>(host->height());
  if (width == hostWidth && height == hostHeight && !host->isFirstFrame()) {
    return;
  }
  width = hostWidth;
  height = hostHeight;
  status = {};
  drawCount = host->getStartDrawCount();
  maxDrawCountReached = false;
  fpsFont = tgfx::Font(host->getTypeface("default"), FONT_SIZE * host->density());
  for (auto i = 0; i < 3; i++) {
    tgfx::Color color = tgfx::Color::Black();
    color[i] = 1.f;
    paints[i].setColor(color);
    paints[i].setAntiAlias(false);
  }

  startRect = tgfx::Rect::MakeWH(25.f * host->density(), 25.f * host->density());
  rects.resize(MAX_RECT_COUNT);
  std::mt19937 rectRng(18);
  std::mt19937 speedRng(36);
  std::uniform_real_distribution<float> rectDistribution(0, 1);
  std::uniform_real_distribution<float> speedDistribution(-1, 1);
  for (size_t i = 0; i < MAX_RECT_COUNT; i++) {
    const auto size = (5.f + rectDistribution(rectRng) * 20.f) * host->density();
    auto& item = rects[i];
    item.rect.setXYWH(-size, -size, size, size);
    item.speedX = speedDistribution(speedRng) * 5.0f;
    item.speedY = speedDistribution(speedRng) * 5.0f;
  }
}

void ParticleBench::AnimateRects(const AppHost* host) {
  if (!maxDrawCountReached) {
    auto halfDrawInterval = static_cast<int64_t>(500000 / targetFPS);
    auto drawTime = host->lastDrawTime();
    auto idleTime = halfDrawInterval * 2 - drawTime;
    if (idleTime > 0) {
      auto factor = static_cast<double>(idleTime > halfDrawInterval ? drawTime : idleTime) /
                    static_cast<double>(halfDrawInterval);
      if (idleTime > halfDrawInterval) {
        factor *= factor;
      }
      auto step = static_cast<int64_t>(INCREASE_STEP * factor);
      drawCount = std::min(drawCount + static_cast<size_t>(step), MAX_RECT_COUNT);
      host->setDrawCount(drawCount);
    }
  }
  auto startX = host->mouseX();
  auto startY = host->mouseY();
  auto screenRect = tgfx::Rect::MakeWH(width, height);
  if (!screenRect.contains(startX, startY)) {
    startX = screenRect.centerX();
    startY = screenRect.centerY();
  }
  startRect.offsetTo(startX - startRect.width() * 0.5f, startY - startRect.height() * 0.5f);
  for (size_t i = 0; i < drawCount; i++) {
    auto& item = rects[i];
    auto& rect = item.rect;
    if (rect.right <= 0 || rect.left >= width || rect.bottom <= 0 || rect.top >= height) {
      auto offsetX = rect.width() * 0.5f;
      auto offsetY = rect.height() * 0.5f;
      rect.offsetTo(startX - offsetX, startY - offsetY);
    } else {
      rect.offset(item.speedX, item.speedY);
    }
  }
}

void ParticleBench::DrawRects(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < drawCount; i++) {
    auto& item = rects[i];
    canvas->drawRect(item.rect, paints[i % 3]);
  }
  canvas->drawRect(startRect, {});
}

void ParticleBench::DrawStatus(tgfx::Canvas* canvas, const AppHost* host) {
  auto currentTime = tgfx::Clock::Now();
  if (lastFlushTime == -1) {
    lastFlushTime = currentTime;
  }
  auto flushInterval = currentTime - lastFlushTime;
  if (flushInterval > FLUSH_INTERVAL) {
    auto fps = host->currentFPS();
    if (fps > 0.0f) {
      currentFPS = fps;
      auto drawTime = host->averageDrawTime();
      if (!maxDrawCountReached) {
        if (currentFPS < targetFPS - 0.5f &&
            drawTime > static_cast<int64_t>(1000000 / targetFPS) - 2000) {
          maxDrawCountReached = true;
        }
      }
      status.clear();
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(1) << currentFPS;
      status.push_back("FPS: " + oss.str());
      oss.str("");
      oss << std::fixed << std::setprecision(1) << static_cast<float>(drawTime) / 1000.f;
      status.push_back("Time: " + oss.str());
      oss.str("");
      oss << drawCount;
      auto countInfo = oss.str();
      if (maxDrawCountReached) {
        countInfo = "[" + countInfo + "]";
      }
      status.push_back("Rects: " + countInfo);
      if (currentFPS > 59.f) {
        fpsColor = tgfx::Color::Green();
      } else if (currentFPS > 29.f) {
        fpsColor = tgfx::Color{1.f, 1.f, 0.f, 1.f};
      } else {
        fpsColor = tgfx::Color{0.91f, 0.31f, 0.28f, 1.f};
      }
      lastFlushTime = currentTime - (flushInterval % FLUSH_INTERVAL);
    }
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

void ParticleBench::DrawGraphics(tgfx::Canvas* canvas, const AppHost* host) {
  auto graphicType = host->getGraphicType();
  for (size_t i = 0; i < drawCount; i++) {
      auto& item = rects[i];
      auto& rect = item.rect;
      tgfx::Paint paint = paints[i % 3];
      switch (graphicType) {
          case GraphicType::rectangle:
              canvas->drawRect(rect, paint);
              break;
          case GraphicType::round:
              canvas->drawCircle(rect.centerX(), rect.centerY(), rect.width() * 0.5f, paint);
              break;
          case GraphicType::roundedRectangle: {
              float radius = rect.width() * 0.2f;
              canvas->drawRoundRect(rect, radius, radius, paint);
              break;
          }
          case GraphicType::oval: {
            canvas->drawOval(rect, paint);
            break;
          }
          case GraphicType::simpleGraphicBlending: {
              size_t type = i % 4;
              switch (type) {
                  case 0:
                      canvas->drawRect(rect, paint);
                      break;
                  case 1:
                      canvas->drawCircle(rect.centerX(), rect.centerY(), rect.width() * 0.5f, paint);
                      break;
                  case 2:
                      canvas->drawRoundRect(rect, rect.width() * 0.2f, rect.width() * 0.2f, paint);
                      break;
                  case 3:
                      canvas->drawOval(rect, paint);
                      break;
                  default:
                      break;
              }
              break;
          }
          case GraphicType::complexGraphics: {
              const int points = 5;
              const float outerRadius = rect.width() * 0.5f;
              const float innerRadius = outerRadius * 0.382f;
              tgfx::Path path;
              for (int i = 0; i < points * 2; i++) {
                  float radius = (i % 2 == 0) ? outerRadius : innerRadius;
                  float angle = static_cast<float>(i) * PI / points;
                  float x = rect.centerX() + radius * std::sin(angle);
                  float y = rect.centerY() - radius * std::cos(angle);
                  if (i == 0) {
                      path.moveTo(x, y);
                  } else {
                      path.lineTo(x, y);
                  }
              }
              path.close();
              canvas->drawPath(path, paint);
              break;
          }
          default:
            break;
      }
  }

  canvas->drawRect(startRect, {});
}

}  // namespace benchmark
