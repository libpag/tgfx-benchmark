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

#include "TGFXWindow.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Clock.h"
#ifdef TGFX_USE_ANGLE
#include "tgfx/gpu/opengl/egl/EGLWindow.h"
#else
#include "tgfx/gpu/opengl/wgl/WGLWindow.h"
#endif

namespace benchmark {

std::shared_ptr<tgfx::Window> TGFXWindow::createTGFXWindow(HWND hwnd) {
#ifdef TGFX_USE_ANGLE
  return tgfx::EGLWindow::MakeFrom(hwnd);
#else
  return tgfx::WGLWindow::MakeFrom(hwnd);
#endif
}

void TGFXWindow::draw() {
  auto currentTime = tgfx::Clock::Now();
  if (!tgfxWindow) {
    tgfxWindow = createTGFXWindow(windowHandle);
  }
  if (tgfxWindow == nullptr) {
    return;
  }
  RECT rect;
  GetClientRect(windowHandle, &rect);
  auto width = static_cast<int>(rect.right - rect.left);
  auto height = static_cast<int>(rect.bottom - rect.top);
  if (width <= 0 || height <= 0) {
    return;
  }
  auto pixelRatio = getPixelRatio();
  auto sizeChanged = appHost->updateScreen(width, height, pixelRatio);
  if (sizeChanged) {
    surface = nullptr;
  }
  auto device = tgfxWindow->getDevice();
  if (device == nullptr) {
    return;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  if (surface == nullptr) {
    if (lastRecording) {
      context->submit(std::move(lastRecording));
    }
    surface = tgfx::Surface::MakeFrom(context, tgfxWindow);
  }
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->clear({0.87f, 0.87f, 0.87f, 1.0f});
  canvas->save();
  auto numBenches = Bench::Count();
  auto index = (lastDrawIndex % numBenches);
  auto bench = Bench::GetByIndex(index);
  bench->draw(canvas, appHost.get());
  canvas->restore();
  auto recording = context->flush();
  std::swap(lastRecording, recording);
  // Submit the previous frame's recording. context->submit() encodes its command list, hands it
  // to the queue, and then drives Window::onPresent() — for WGL this is SwapBuffers(), which
  // under wglSwapInterval(1) may block when the driver's swap queue fills up. Use
  // Context::lastPresentTime() to obtain the exact onPresent() duration (instrumented in the
  // bundled tgfx) and subtract it so the vsync wait does not pollute the per-frame draw time.
  int64_t presentTime = 0;
  if (recording != nullptr) {
    context->submit(std::move(recording));
    presentTime = context->lastPresentTime();
  }
  device->unlock();
  auto drawTime = tgfx::Clock::Now() - currentTime - presentTime;
  appHost->recordFrame(drawTime);
}
}  // namespace benchmark
