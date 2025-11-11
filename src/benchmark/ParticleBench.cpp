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

#include "ParticleBench.h"
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>
#include "tgfx/core/Clock.h"

namespace benchmark {
static constexpr int64_t FLUSH_INTERVAL = 300000;
static constexpr float FPS_BACKGROUND_HEIGHT = 50.f;
static constexpr float STATUS_WIDTH = 250.f;
static constexpr float FONT_SIZE = 40.f;

static bool DrawStatusFlag = true;
static size_t InitDrawCount = 1;
static float TargetFPS = 60.0f;
static size_t MaxDrawCount = 1000000;
static size_t IncreaseStep = 1000;
static bool AntiAliasFlag = true;
static bool StrokeFlag = true;
static tgfx::LineJoin LineJoinType = tgfx::LineJoin::Miter;

static std::string ToString(GraphicType type) {
  switch (type) {
    case GraphicType::Rect:
      return "Rect";
    case GraphicType::Circle:
      return "Circle";
    case GraphicType::RRect:
      return "RRect";
    case GraphicType::Oval:
      return "Oval";
    case GraphicType::Star:
      return "Star";
    default:
      return "Unknown";
  }
}

ParticleBench::ParticleBench(GraphicType type)
    : Bench("ParticleBench-" + ToString(type)), graphicType(type) {
}

void ParticleBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  Init(host);
  AnimateRects(host);
  DrawGraphics(canvas);
  DrawStatus(canvas, host);
}

static tgfx::Path CreateStar(const tgfx::Rect& rect) {
  const int points = 5;
  const float outerRadius = rect.width() * 0.5f;
  const float innerRadius = outerRadius * 0.382f;
  tgfx::Path path;
  const float angleStep = static_cast<float>(M_PI) / points;
  const float centerX = rect.centerX();
  const float centerY = rect.centerY();
  for (int j = 0; j < points * 2; j++) {
    const float radius = (j % 2 == 0) ? outerRadius : innerRadius;
    const float angle = static_cast<float>(j) * angleStep;
    const float x = centerX + radius * std::sin(angle);
    const float y = centerY - radius * std::cos(angle);
    if (j == 0) {
      path.moveTo(x, y);
    } else {
      path.lineTo(x, y);
    }
  }
  path.close();
  return path;
}

void ParticleBench::Init(const AppHost* host) {
  auto hostWidth = static_cast<float>(host->width());
  auto hostHeight = static_cast<float>(host->height());
  if (width == hostWidth && height == hostHeight && !host->isFirstFrame()) {
    return;
  }
  width = hostWidth;
  height = hostHeight;
  status = {};
  drawCount = InitDrawCount;
  maxDrawCountReached = false;
  perfData = {};
  fpsFont = tgfx::Font(host->getTypeface("default"), FONT_SIZE * host->density());
  for (auto i = 0; i < 3; i++) {
    tgfx::Color color = tgfx::Color::Black();
    color[i] = 1.f;
    paints[i].setColor(color);
    paints[i].setAntiAlias(AntiAliasFlag);
    if (StrokeFlag) {
      paints[i].setStyle(tgfx::PaintStyle::Stroke);
      paints[i].setStrokeWidth(4.0f);
      paints[i].setLineJoin(LineJoinType);
    } else {
      paints[i].setStyle(tgfx::PaintStyle::Fill);
    }
  }

  startRect = tgfx::Rect::MakeWH(20.f * host->density(), 20.f * host->density());
  graphics.resize(MaxDrawCount);
  std::mt19937 rectRng(18);
  std::mt19937 speedRng(36);
  std::uniform_real_distribution<float> rectDistribution(0, 1);
  std::uniform_real_distribution<float> speedDistribution(-1, 1);
  for (size_t i = 0; i < MaxDrawCount; i++) {
    const auto size = (4.f + rectDistribution(rectRng) * 10.f) * host->density();
    auto& graphic = graphics[i];
    if (graphicType == GraphicType::Oval) {
      graphic.rect.setXYWH(-size, -size, size, 0.8f * size);
    } else {
      graphic.rect.setXYWH(-size, -size, size, size);
    }
    graphic.speedX = speedDistribution(speedRng) * 5.0f;
    graphic.speedY = speedDistribution(speedRng) * 5.0f;
  }
  if (graphicType == GraphicType::Star) {
    paths.resize(MaxDrawCount);
    for (size_t i = 0; i < MaxDrawCount; i++) {
      paths[i] = CreateStar(graphics[i].rect);
    }
  }
}

void ParticleBench::AnimateRects(const AppHost* host) {
  if (!maxDrawCountReached) {
    auto halfDrawInterval = static_cast<int64_t>(500000 / TargetFPS);
    auto drawTime = host->lastDrawTime();
    auto idleTime = halfDrawInterval * 2 - drawTime;
    if (idleTime > 0) {
      auto factor = static_cast<double>(idleTime > halfDrawInterval ? drawTime : idleTime) /
                    static_cast<double>(halfDrawInterval);
      auto step = static_cast<int64_t>(IncreaseStep * factor);
      if (step < 1) {
        step = 1;
      }
      drawCount = std::min(drawCount + static_cast<size_t>(step), MaxDrawCount);
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
    auto& graphic = graphics[i];
    auto& rect = graphic.rect;
    if (rect.right <= 0 || rect.left >= width || rect.bottom <= 0 || rect.top >= height) {
      auto offsetX = rect.width() * 0.5f;
      auto offsetY = rect.height() * 0.5f;
      rect.offsetTo(startX - offsetX, startY - offsetY);
    } else {
      rect.offset(graphic.speedX, graphic.speedY);
    }
  }
}

void ParticleBench::DrawRects(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < drawCount; i++) {
    auto& graphic = graphics[i];
    canvas->drawRect(graphic.rect, paints[i % 3]);
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
        if ((currentFPS < TargetFPS - 0.5f &&
             drawTime > static_cast<int64_t>(1000000 / TargetFPS) - 2000) ||
            drawCount >= MaxDrawCount) {
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
      status.push_back("Count: " + countInfo);
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
  perfData.fps = currentFPS;
  perfData.drawTime = static_cast<float>(host->averageDrawTime()) / 1000.f;
  perfData.drawCount = drawCount;
  if (!DrawStatusFlag) {
    return;
  }
  canvas->resetMatrix();
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

void ParticleBench::DrawCircle(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < drawCount; i++) {
    auto& graphic = graphics[i];
    auto& rect = graphic.rect;
    auto& paint = paints[i % 3];
    canvas->drawCircle(rect.centerX(), rect.centerY(), rect.width() * 0.5f, paint);
  }
  canvas->drawRect(startRect, {});
}

void ParticleBench::DrawRRect(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < drawCount; i++) {
    auto& graphic = graphics[i];
    auto& rect = graphic.rect;
    auto& paint = paints[i % 3];
    const float radius = rect.width() * 0.25f;
    canvas->drawRoundRect(rect, radius, radius, paint);
  }
  canvas->drawRect(startRect, {});
}

void ParticleBench::DrawOval(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < drawCount; i++) {
    auto& item = graphics[i];
    auto& rect = item.rect;
    auto& paint = paints[i % 3];
    canvas->drawOval(rect, paint);
  }
  canvas->drawRect(startRect, {});
}

void ParticleBench::DrawStar(tgfx::Canvas* canvas) const {
  for (size_t i = 0; i < drawCount; i++) {
    auto& graphic = graphics[i];
    canvas->setMatrix(tgfx::Matrix::MakeTrans(graphic.rect.centerX(), graphic.rect.centerY()));
    auto& paint = paints[i % 3];
    canvas->drawPath(paths[i], paint);
  }
  canvas->resetMatrix();
  canvas->drawRect(startRect, {});
}

void ParticleBench::DrawGraphics(tgfx::Canvas* canvas) const {
  switch (graphicType) {
    case GraphicType::Rect:
      DrawRects(canvas);
      break;
    case GraphicType::Circle:
      DrawCircle(canvas);
      break;
    case GraphicType::RRect:
      DrawRRect(canvas);
      break;
    case GraphicType::Oval:
      DrawOval(canvas);
      break;
    case GraphicType::Star:
      DrawStar(canvas);
      break;
    default:
      DrawRects(canvas);
      break;
  }
}

void ParticleBench::ShowPerfData(bool status) {
  DrawStatusFlag = status;
}

void ParticleBench::SetInitDrawCount(size_t count) {
  InitDrawCount = std::max(static_cast<size_t>(1), count);
}

void ParticleBench::SetMaxDrawCount(size_t count) {
  MaxDrawCount = count;
}

void ParticleBench::SetStepDrawCount(size_t count) {
  IncreaseStep = count;
}

void ParticleBench::SetTargetFPS(float fps) {
  TargetFPS = fps;
}

bool ParticleBench::isMaxDrawCountReached() const {
  return maxDrawCountReached;
}

PerfData ParticleBench::getPerfData() const {
  return perfData;
}

void ParticleBench::SetAntiAlias(bool aa) {
  AntiAliasFlag = aa;
}

void ParticleBench::SetStroke(bool stroke) {
  StrokeFlag = stroke;
}
}  // namespace benchmark