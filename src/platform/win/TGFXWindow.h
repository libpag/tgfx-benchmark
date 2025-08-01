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

#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>
#include <Windowsx.h>
#include <functional>
#include <memory>
#include <string>
#include "base/Bench.h"
#ifdef TGFX_USE_ANGLE
#include "tgfx/gpu/opengl/egl/EGLWindow.h"
#else
#include "tgfx/gpu/opengl/wgl/WGLWindow.h"
#endif

namespace benchmark {
class TGFXWindow {
 public:
  TGFXWindow();
  virtual ~TGFXWindow();

  bool open();

 private:
  HWND windowHandle = nullptr;
  int lastDrawIndex = 0;
  std::shared_ptr<AppHost> appHost = nullptr;
#ifdef TGFX_USE_ANGLE
  std::shared_ptr<tgfx::EGLWindow> tgfxWindow = nullptr;
#else
  std::shared_ptr<tgfx::WGLWindow> tgfxWindow = nullptr;
#endif

  static WNDCLASS RegisterWindowClass();
  static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

  LRESULT handleMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

  void destroy();
  void centerAndShow();
  float getPixelRatio();
  void createAppHost();
  void draw();
};
}  // namespace benchmark
