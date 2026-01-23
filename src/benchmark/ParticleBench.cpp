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
#include <iostream>
#include <random>
#include <sstream>
#include "tgfx/core/Clock.h"
#include "tgfx/core/UTF.h"

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
static bool StrokeFlag = false;
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
    case GraphicType::Text:
      return "Text";
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

static GlyphRunData CreateGlyphRun(const AppHost* host, std::vector<GraphicData>& glyphDatas) {
  constexpr std::string_view LONG_TEXT =
      R"(君子曰：学不可以已。青，取之于蓝，而青于蓝；冰，水为之，而寒于水。木直中绳，
𫐓以为轮，其曲中规。虽有槁暴，不复挺者，𫐓使之然也。故木受绳则直，金就砺则利，君子博学而日参省乎己，则知明而行无过矣。
故不登高山，不知天之高也；不临深溪，不知地之厚也；不闻先王之遗言，不知学问之大也。干、越、夷、貉之子，生而同声，
长而异俗，教使之然也。诗曰："嗟尔君子，无恒安息。靖共尔位，好是正直。神之听之，介尔景福。"神莫大于化道，福莫长于无祸。
（此段教材无）吾尝终日而思矣，不如须臾之所学也；吾尝跂而望矣，不如登高之博见也。登高而招，臂非加长也，而见者远；顺风而呼，
声非加疾也，而闻者彰。假舆马者，非利足也，而致千里；假舟楫者，非能水也，而绝江河。君子生非异也，善假于物也。南方有鸟焉，
名曰蒙鸠，以羽为巢，而编之以发，系之苇苕，风至苕折，卵破子死。巢非不完也，所系者然也。西方有木焉，名曰射干，茎长四寸，
生于高山之上，而临百仞之渊，木茎非能长也，所立者然也。)";

  std::vector<tgfx::GlyphID> sourceGlyphIDs = {};
  const char* textStart = LONG_TEXT.data();
  const char* textStop = textStart + LONG_TEXT.size();
  auto font = tgfx::Font(host->getTypeface("default"), 10.f * host->density());
  while (textStart != textStop) {
    auto unichar = tgfx::UTF::NextUTF8(&textStart, textStop);
    auto glyphID = font.getGlyphID(unichar);
    if (glyphID == 0) {
      continue;
    }
    sourceGlyphIDs.push_back(glyphID);
  }
  if (sourceGlyphIDs.empty()) {
    return {};
  }

  //auto centerX = static_cast<float>(host->width()) * 0.5f;
  //auto centerY = static_cast<float>(host->height()) * 0.5f;
  std::vector<tgfx::GlyphID> glyphIDs = {};
  std::vector<tgfx::Point> positions = {};
  const auto totalCount = glyphDatas.size();
  const auto sourceGlyphCount = sourceGlyphIDs.size();
  for (size_t i = 0; i < totalCount; i++) {
    //glyphDatas[i].rect.offsetTo(centerX, centerY);
    glyphIDs.push_back(sourceGlyphIDs[i % sourceGlyphCount]);
    positions.emplace_back(glyphDatas[i].rect.left, glyphDatas[i].rect.top);
  }
  return {font, std::move(glyphIDs), std::move(positions)};
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
      paints[i].setStrokeWidth(2.0f);
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
  if (graphicType == GraphicType::Text) {
    glyphRun = CreateGlyphRun(host, graphics);
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
  tgfx::Rect border{0, 0, width, height};
  if (graphicType == GraphicType::Text) {
    // Expand border for text clipping test
    border.outset(width, height);
  }
  for (size_t i = 0; i < drawCount; i++) {
    auto& graphic = graphics[i];
    auto& rect = graphic.rect;
    if (!tgfx::Rect::Intersects(rect, border)) {
      auto offsetX = rect.width() * 0.5f;
      auto offsetY = rect.height() * 0.5f;
      rect.offsetTo(startX - offsetX, startY - offsetY);
    } else {
      rect.offset(graphic.speedX, graphic.speedY);
    }
    if (graphicType == GraphicType::Text) {
      glyphRun.positions[i].set(rect.left, rect.top);
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

void ParticleBench::DrawText(tgfx::Canvas* canvas) const {
  const auto& glyphs = glyphRun.glyphs;
  const auto& positions = glyphRun.positions;
  auto groupSize = static_cast<int>(drawCount / 3);
  for (auto i = 0; i < 3; i++) {
    auto start = i * groupSize;
    auto end = (i == 2) ? static_cast<int>(drawCount) : (i + 1) * groupSize;
    auto count = static_cast<size_t>(end - start);
    auto textBlob = tgfx::TextBlob::MakeFrom(glyphs.data() + start, positions.data() + start, count,
                                             glyphRun.font);
    canvas->drawTextBlob(std::move(textBlob), 0.f, 0.f, paints[i]);
  }
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
    case GraphicType::Text:
      DrawText(canvas);
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
