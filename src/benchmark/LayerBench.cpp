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

#include "LayerBench.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>
#include "tgfx/core/Clock.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/SolidLayer.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "tgfx/layers/layerstyles/InnerShadowStyle.h"

namespace benchmark {
static constexpr int64_t FLUSH_INTERVAL = 300000;
static constexpr float FPS_BACKGROUND_HEIGHT = 50.f;
static constexpr float STATUS_WIDTH = 250.f;
static constexpr float FONT_SIZE = 40.f;
static constexpr float NAME_FONT_SIZE = 16.f;
static constexpr float NAME_LEFT_PADDING = 10.f;
static constexpr float NAME_MAX_WIDTH = 95.f;

static bool DrawStatusFlag = true;
static size_t InitDrawCount = 1;
static float TargetFPS = 60.0f;
static size_t MaxDrawCount = 1000000;
static size_t IncreaseStep = 20;

static std::string ToString(LayerBenchType type) {
  switch (type) {
    case LayerBenchType::DropShadow:
      return "DropShadow";
    case LayerBenchType::InnerShadow:
      return "InnerShadow";
    default:
      return "Unknown";
  }
}

static std::vector<std::string> WrapText(const std::string& text, const tgfx::Font& font,
                                         float maxWidth) {
  std::vector<std::string> lines;
  std::string current;
  float currentWidth = 0.f;
  for (char c : text) {
    auto glyphID = font.getGlyphID(static_cast<tgfx::Unichar>(c));
    auto advance = font.getAdvance(glyphID);
    if (!current.empty() && currentWidth + advance > maxWidth) {
      lines.push_back(current);
      current.clear();
      currentWidth = 0.f;
    }
    current.push_back(c);
    currentWidth += advance;
  }
  if (!current.empty()) {
    lines.push_back(current);
  }
  return lines;
}

LayerBench::LayerBench(LayerBenchType type)
    : Bench("LayerBench-" + ToString(type)), benchType(type) {
}

void LayerBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  Init(host);
  SyncParticles(host);
  AnimateParticles(host);
  rootLayer->draw(canvas);
  DrawStatus(canvas, host);
}

void LayerBench::Init(const AppHost* host) {
  auto hostWidth = static_cast<float>(host->width());
  auto hostHeight = static_cast<float>(host->height());
  if (width == hostWidth && height == hostHeight && !host->isFirstFrame()) {
    return;
  }
  width = hostWidth;
  height = hostHeight;
  density = host->density();
  status = {};
  drawCount = InitDrawCount;
  maxDrawCountReached = false;
  perfData = {};
  fpsFont = tgfx::Font(host->getTypeface("default"), FONT_SIZE * density);
  nameFont = tgfx::Font(host->getTypeface("default"), NAME_FONT_SIZE * density);

  particleData.resize(MaxDrawCount);
  std::mt19937 rectRng(18);
  std::mt19937 speedRng(36);
  std::uniform_real_distribution<float> rectDistribution(0, 1);
  std::uniform_real_distribution<float> speedDistribution(-1, 1);
  for (size_t i = 0; i < MaxDrawCount; i++) {
    auto& data = particleData[i];
    data.size = (4.f + rectDistribution(rectRng) * 10.f) * density;
    data.speedX = speedDistribution(speedRng) * 5.0f;
    data.speedY = speedDistribution(speedRng) * 5.0f;
  }
  particles.clear();
  rootLayer = tgfx::Layer::Make();
  rootLayer->removeChildren();
}

std::shared_ptr<tgfx::SolidLayer> LayerBench::CreateParticle(size_t index) const {
  const auto& data = particleData[index];
  auto layer = tgfx::SolidLayer::Make();
  layer->setWidth(data.size);
  layer->setHeight(data.size);
  layer->setPosition({-data.size, -data.size});
  tgfx::Color color = tgfx::Color::Black();
  color[index % 3] = 1.f;
  layer->setColor(color);
  const auto radius = (index % 2 == 0) ? 0.f : data.size * 0.25f;
  layer->setRadiusX(radius);
  layer->setRadiusY(radius);
  const auto shortSide = data.size;
  const auto blur = shortSide * 0.25f;
  const auto offset = shortSide * 0.25f;
  const auto spread = shortSide * 0.125f;
  const tgfx::Color shadowColor{1.f, 0.f, 0.f, 0.5f};
  if (benchType == LayerBenchType::DropShadow) {
    auto style = tgfx::DropShadowStyle::Make(offset, offset, blur, blur, shadowColor);
    style->setSpread(spread);
    layer->setLayerStyles({style});
  } else {
    auto style = tgfx::InnerShadowStyle::Make(offset, offset, blur, blur, shadowColor);
    style->setSpread(spread);
    layer->setLayerStyles({style});
  }
  return layer;
}

void LayerBench::SyncParticles(const AppHost* host) {
  if (!maxDrawCountReached) {
    auto halfDrawInterval = static_cast<int64_t>(500000 / TargetFPS);
    auto drawTime = host->lastDrawTime();
    auto idleTime = halfDrawInterval * 2 - drawTime;
    if (idleTime > 0) {
      auto factor = static_cast<double>(idleTime > halfDrawInterval ? drawTime : idleTime) /
                    static_cast<double>(halfDrawInterval);
      auto step = static_cast<int64_t>(static_cast<double>(IncreaseStep) * factor);
      if (step < 1) {
        step = 1;
      }
      drawCount = std::min(drawCount + static_cast<size_t>(step), MaxDrawCount);
    }
  }
  while (particles.size() < drawCount) {
    auto particle = CreateParticle(particles.size());
    particles.push_back(particle);
    rootLayer->addChild(particle);
  }
}

void LayerBench::AnimateParticles(const AppHost* host) {
  auto startX = host->mouseX();
  auto startY = host->mouseY();
  auto screenRect = tgfx::Rect::MakeWH(width, height);
  if (!screenRect.contains(startX, startY)) {
    startX = screenRect.centerX();
    startY = screenRect.centerY();
  }
  for (size_t i = 0; i < drawCount; i++) {
    auto& particle = particles[i];
    const auto& data = particleData[i];
    const auto bounds = particle->getBounds(rootLayer.get());
    if (bounds.right <= 0 || bounds.left >= width || bounds.bottom <= 0 || bounds.top >= height) {
      const auto size = particle->width();
      particle->setPosition({startX - size * 0.5f, startY - size * 0.5f});
    } else {
      const auto pos = particle->position();
      particle->setPosition({pos.x + data.speedX, pos.y + data.speedY});
    }
  }
}

void LayerBench::DrawStatus(tgfx::Canvas* canvas, const AppHost* host) {
  auto currentTime = tgfx::Clock::Now();
  if (lastFlushTime == -1) {
    lastFlushTime = currentTime;
  }
  auto flushInterval = currentTime - lastFlushTime;
  if (flushInterval > FLUSH_INTERVAL) {
    auto fps = host->currentFPS();
    if (fps > 0.0f) {
      _currentFPS = fps;
      auto drawTime = host->averageDrawTime();
      if (!maxDrawCountReached) {
        if ((_currentFPS < TargetFPS - 0.5f &&
             drawTime > static_cast<int64_t>(1000000 / TargetFPS) - 2000) ||
            drawCount >= MaxDrawCount) {
          maxDrawCountReached = true;
        }
      }
      status.clear();
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(1) << _currentFPS;
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
      if (_currentFPS > 59.f) {
        fpsColor = tgfx::Color::Green();
      } else if (_currentFPS > 29.f) {
        fpsColor = tgfx::Color{1.f, 1.f, 0.f, 1.f};
      } else {
        fpsColor = tgfx::Color{0.91f, 0.31f, 0.28f, 1.f};
      }
      lastFlushTime = currentTime - (flushInterval % FLUSH_INTERVAL);
    }
  }
  perfData.fps = _currentFPS;
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
  paint.setColor(tgfx::Color::White());
  auto nameLines = WrapText(name(), nameFont, NAME_MAX_WIDTH * host->density());
  const auto lineHeight = NAME_FONT_SIZE * host->density();
  const auto lineGap = 2.f * host->density();
  const auto totalHeight = static_cast<float>(nameLines.size()) * lineHeight +
                           static_cast<float>(nameLines.size() - 1) * lineGap;
  auto nameTop = (FPS_BACKGROUND_HEIGHT * host->density() - totalHeight) * 0.5f + lineHeight;
  for (const auto& line : nameLines) {
    canvas->drawSimpleText(line, NAME_LEFT_PADDING * host->density(), nameTop, nameFont, paint);
    nameTop += lineHeight + lineGap;
  }
  paint.setColor(fpsColor);
  float left = STATUS_WIDTH * host->density() / 2;
  for (auto& line : status) {
    canvas->drawSimpleText(line, left, top, fpsFont, paint);
    left += STATUS_WIDTH * host->density();
  }
}

void LayerBench::ShowPerfData(bool flag) {
  DrawStatusFlag = flag;
}

void LayerBench::SetInitDrawCount(size_t count) {
  InitDrawCount = std::max(static_cast<size_t>(1), count);
}

void LayerBench::SetMaxDrawCount(size_t count) {
  MaxDrawCount = count;
}

void LayerBench::SetStepDrawCount(size_t count) {
  IncreaseStep = count;
}

void LayerBench::SetTargetFPS(float fps) {
  TargetFPS = fps;
}

bool LayerBench::isMaxDrawCountReached() const {
  return maxDrawCountReached;
}

PerfData LayerBench::getPerfData() const {
  return perfData;
}
}  // namespace benchmark
