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

#include "TGFXBaseView.h"
#include <emscripten/html5.h>
#include <emscripten/val.h>
#include <iostream>
#include "base/Bench.h"
#include "tgfx/core/Clock.h"

using namespace emscripten;

static constexpr int64_t FLUSH_INTERVAL = 300000;

namespace benchmark {
EM_BOOL RequestFrameCallback(double, void* userData) {
  auto baseView = static_cast<TGFXBaseView*>(userData);
  if (baseView) {
    baseView->draw();
  }
  return EM_TRUE;
}

EM_BOOL MouseClickCallback(int, const EmscriptenMouseEvent* e, void* userData) {
  auto baseView = static_cast<TGFXBaseView*>(userData);
  if (baseView) {
    double devicePixelRatio = emscripten_get_device_pixel_ratio();
    float x = static_cast<float>(devicePixelRatio) * static_cast<float>(e->clientX);
    float y = static_cast<float>(devicePixelRatio) * static_cast<float>(e->clientY);
    baseView->appHost->mouseMoved(x, y);
    baseView->appHost->resetFrames();
    baseView->drawIndex++;
  }
  return EM_TRUE;
}

EM_BOOL MouseMoveCallBack(int, const EmscriptenMouseEvent* e, void* userData) {
  auto appHost = static_cast<benchmark::AppHost*>(userData);
  if (appHost) {
    double devicePixelRatio = emscripten_get_device_pixel_ratio();
    float x = static_cast<float>(devicePixelRatio) * static_cast<float>(e->clientX);
    float y = static_cast<float>(devicePixelRatio) * static_cast<float>(e->clientY);
    appHost->mouseMoved(x, y);
  }
  return EM_TRUE;
}

EM_BOOL MouseLeaveCallBack(int, const EmscriptenMouseEvent*, void* userData) {
  auto appHost = static_cast<benchmark::AppHost*>(userData);
  if (appHost) {
    appHost->mouseMoved(-1, -1);
  }
  return EM_TRUE;
}

TGFXBaseView::TGFXBaseView(const std::string& canvasID) : canvasID(canvasID) {
  appHost = std::make_shared<benchmark::AppHost>(1024, 720);
  drawIndex = 0;
  emscripten_set_click_callback(canvasID.c_str(), this, EM_TRUE, MouseClickCallback);
  emscripten_set_mousemove_callback(canvasID.c_str(), appHost.get(), EM_TRUE, MouseMoveCallBack);
  emscripten_set_mouseleave_callback(canvasID.c_str(), appHost.get(), EM_TRUE, MouseLeaveCallBack);
}

void TGFXBaseView::updateSize(float devicePixelRatio) {
  if (!canvasID.empty()) {
    int width = 0;
    int height = 0;
    emscripten_get_canvas_element_size(canvasID.c_str(), &width, &height);
    auto sizeChanged = appHost->updateScreen(width, height, devicePixelRatio);
    if (sizeChanged && window) {
      window->invalidSize();
    }
  }
}

void TGFXBaseView::startDraw() {
  ParticleBench::ShowPerfData(showPerfDataFlag);
  emscripten_request_animation_frame_loop(RequestFrameCallback, this);
}

void TGFXBaseView::setImagePath(const std::string& imagePath) {
  auto image = tgfx::Image::MakeFromFile(imagePath.c_str());
  if (image) {
    appHost->addImage("bridge", std::move(image));
  }
}

void TGFXBaseView::draw() {
  auto currentTime = tgfx::Clock::Now();
  if (appHost->width() <= 0 || appHost->height() <= 0) {
    return;
  }
  if (window == nullptr) {
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
  }
  if (window == nullptr) {
    return;
  }
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  auto surface = window->getSurface(context);
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->clear({0.87f, 0.87f, 0.87f, 1.0f});
  auto numBenches = benchmark::Bench::Count();
  auto index = (drawIndex % numBenches);
  auto bench = benchmark::Bench::GetByIndex(index);
  bench->draw(canvas, appHost.get());
  const auto particleBench = static_cast<benchmark::ParticleBench*>(bench);

  if (!showPerfDataFlag) {
    updatePerfInfo(particleBench->getPerfData());
  }
  context->flushAndSubmit();
  window->present(context);
  device->unlock();
  auto drawTime = tgfx::Clock::Now() - currentTime;
  appHost->recordFrame(drawTime);
}

void TGFXBaseView::restartDraw() const {
  if (appHost) {
    appHost->resetFrames();
  }
}

void TGFXBaseView::updatePerfInfo(const PerfData& data) {
  auto jsWindow = emscripten::val::global("window");
  if (!jsWindow.hasOwnProperty("updatePerfInfo")) {
    return;
  }
  static int64_t lastFlushTime = -1;
  const auto currentTime = tgfx::Clock::Now();
  if (lastFlushTime == -1) {
    lastFlushTime = currentTime;
  }
  if (data.fps > 0.0f) {
    if (const auto flushInterval = currentTime - lastFlushTime; flushInterval > FLUSH_INTERVAL) {
      const auto bench = getBenchByIndex();
      jsWindow.call<void>("updatePerfInfo", data.fps, data.drawTime, data.drawCount,
                          bench->isMaxDrawCountReached());
      lastFlushTime = currentTime - (flushInterval % FLUSH_INTERVAL);
    }
  }
}

void TGFXBaseView::updateDrawParam(const DrawParam& drawParam) const {
  ParticleBench::SetInitDrawCount(drawParam.startCount);
  ParticleBench::SetStepDrawCount(drawParam.stepCount);
  ParticleBench::SetMaxDrawCount(drawParam.maxCount);
  ParticleBench::SetTargetFPS(drawParam.minFPS);
  appHost->resetFrames();
}

void TGFXBaseView::updateGraphicType(int type) {
  drawIndex = type;
  appHost->resetFrames();
}

ParticleBench* TGFXBaseView::getBenchByIndex() const {
  const auto numBenches = benchmark::Bench::Count();
  const auto index = (drawIndex % numBenches);
  const auto bench = benchmark::Bench::GetByIndex(index);
  return static_cast<ParticleBench*>(bench);
}

void TGFXBaseView::showPerfData(bool show) {
  showPerfDataFlag = show;
}

void TGFXBaseView::setAntiAlias(bool aa) {
  ParticleBench::SetAntiAlias(aa);
  appHost->resetFrames();
}

}  // namespace benchmark

int main() {
  return 0;
}