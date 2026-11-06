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

#include "base/Bench.h"
#include <algorithm>
#include <unordered_map>
#include "benchmark/ParticleBench.h"
#include "benchmark/MotionMarkBenches.h"
#include "tgfx/core/Clock.h"
#include "tgfx/platform/Print.h"

namespace benchmark {
static ParticleBench rectParticles(GraphicType::Rect);
static ParticleBench circleParticles(GraphicType::Circle);
static ParticleBench ovalParticles(GraphicType::Oval);
static ParticleBench rrectParticles(GraphicType::RRect);
static ParticleBench starParticles(GraphicType::Star);
static MultiplyBench multiplyBench;
static CanvasArcsBench canvasArcsBench;
static LeavesBench leavesBench;
static CanvasLinesBench canvasLinesBench;
static ImagesBench imagesBench;
static DesignBench designBench;
static SuitsBench suitsBench;
static ShaderMixBench shaderMixBench;

static std::vector<Bench*> drawers = {
    &rectParticles, &circleParticles, &ovalParticles, &rrectParticles, &starParticles,
    &multiplyBench, &canvasArcsBench, &leavesBench,
    &canvasLinesBench, &imagesBench, &designBench, &suitsBench,
    &shaderMixBench};

static std::vector<std::string> GetDrawerNames() {
  std::vector<std::string> names;
  for (const auto& drawer : drawers) {
    names.push_back(drawer->name());
  }
  return names;
}

static std::unordered_map<std::string, Bench*> GetDrawerMap() {
  std::unordered_map<std::string, Bench*> map;
  for (const auto& drawer : drawers) {
    map[drawer->name()] = drawer;
  }
  return map;
}

static std::unordered_map<std::string, Bench*> GetStableIdMap() {
  std::unordered_map<std::string, Bench*> map;
  map["legacy.particles-rect"] = drawers[static_cast<size_t>(GraphicType::Rect)];
  map["legacy.particles-circle"] = drawers[static_cast<size_t>(GraphicType::Circle)];
  map["legacy.particles-oval"] = drawers[static_cast<size_t>(GraphicType::Oval)];
  map["legacy.particles-rrect"] = drawers[static_cast<size_t>(GraphicType::RRect)];
  map["legacy.particles-star"] = &starParticles;
  map["motionmark.multiply"] = &multiplyBench;
  map["motionmark.canvas-arcs"] = &canvasArcsBench;
  map["motionmark.leaves"] = &leavesBench;
  map["motionmark.canvas-lines"] = &canvasLinesBench;
  map["motionmark.images"] = &imagesBench;
  map["motionmark.design"] = &designBench;
  map["motionmark.suits"] = &suitsBench;
  map["coldstart.shader-mix"] = &shaderMixBench;
  return map;
}

int Bench::Count() {
  return static_cast<int>(drawers.size());
}

const std::vector<std::string>& Bench::Names() {
  static auto names = GetDrawerNames();
  return names;
}

Bench* Bench::GetByIndex(int index) {
  if (index < 0 || index >= Count()) {
    return nullptr;
  }
  return drawers[static_cast<size_t>(index)];
}

Bench* Bench::GetByName(const std::string& name) {
  static auto drawerMap = GetDrawerMap();
  auto it = drawerMap.find(name);
  if (it == drawerMap.end()) {
    return nullptr;
  }
  return it->second;
}

Bench* Bench::GetByStableId(const std::string& stableId) {
  static auto stableIdMap = GetStableIdMap();
  auto it = stableIdMap.find(stableId);
  if (it == stableIdMap.end()) {
    return nullptr;
  }
  return it->second;
}

Bench* Bench::Resolve(const std::string& identifier) {
  auto bench = GetByStableId(identifier);
  return bench != nullptr ? bench : GetByName(identifier);
}

Bench::Bench(std::string name) : _name(std::move(name)) {
}

void Bench::draw(tgfx::Canvas* canvas, const AppHost* host) {
  if (canvas == nullptr) {
    tgfx::PrintError("Drawer::draw() canvas is nullptr!");
    return;
  }
  if (host == nullptr) {
    tgfx::PrintError("Drawer::draw() appHost is nullptr!");
    return;
  }
  canvas->save();
  const auto nowUs = tgfx::Clock::Now();
  animationDelta = lastAnimationUs == 0 ? 1.0f
      : static_cast<float>(std::clamp<int64_t>(nowUs - lastAnimationUs, 0, 100000)) * 0.00006f;
  lastAnimationUs = nowUs;
  animationFrame += animationDelta;
  ++frame;
  onDraw(canvas, host);
  canvas->restore();
}
}  // namespace benchmark
