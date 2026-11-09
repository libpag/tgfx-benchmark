/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "TGFXWindow.h"
#include "base/AppHost.h"
#include "base/Bench.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/gpu/Recording.h"
#include "tgfx/gpu/Window.h"
#include "tgfx/gpu/opengl/egl/EGLWindow.h"

namespace benchmark {

TGFXWindow::~TGFXWindow() {
  close();
}

bool TGFXWindow::open(ANativeWindow* nativeWindow) {
  if (nativeWindow == nullptr) {
    return false;
  }
  close();
  _appHost = std::make_unique<AppHost>();
  auto typeface = tgfx::Typeface::MakeFromName("Noto Sans CJK SC", "");
  _appHost->addTypeface("default", typeface);
  _tgfxWindow = tgfx::EGLWindow::MakeFrom(nativeWindow);
  _drawIndex = 0;
  return _tgfxWindow != nullptr;
}

void TGFXWindow::close() {
  _lastRecording = nullptr;
  _surface = nullptr;
  _tgfxWindow = nullptr;
  _appHost = nullptr;
}

void TGFXWindow::resize(int width, int height) {
  if (_appHost == nullptr || width <= 0 || height <= 0) {
    return;
  }
  if (_appHost->updateScreen(width, height, 1.0f)) {
    _surface = nullptr;
  }
}

void TGFXWindow::draw() {
  if (_tgfxWindow == nullptr || _appHost == nullptr) {
    return;
  }
  if (_appHost->width() <= 0 || _appHost->height() <= 0) {
    return;
  }
  auto device = _tgfxWindow->getDevice();
  if (device == nullptr) {
    return;
  }
  auto currentTime = tgfx::Clock::Now();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  if (_surface == nullptr) {
    if (_lastRecording != nullptr) {
      context->submit(std::move(_lastRecording));
    }
    _surface = tgfx::Surface::MakeFrom(context, _tgfxWindow);
  }
  if (_surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = _surface->getCanvas();
  canvas->clear({0.87f, 0.87f, 0.87f, 1.0f});
  auto count = Bench::Count();
  auto bench = Bench::GetByIndex(static_cast<int>(_drawIndex % count));
  bench->draw(canvas, _appHost.get());
  auto recording = context->flush();
  std::swap(_lastRecording, recording);
  if (recording != nullptr) {
    context->submit(std::move(recording));
  }
  device->unlock();
  _appHost->recordFrame(tgfx::Clock::Now() - currentTime);
  _drawIndex++;
}

}  // namespace benchmark
