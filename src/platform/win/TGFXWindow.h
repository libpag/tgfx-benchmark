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
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Recording.h"
#include "tgfx/gpu/Window.h"

namespace benchmark {
class TGFXWindow {
 public:
  TGFXWindow();
  virtual ~TGFXWindow();

  bool open();

 private:
  HWND windowHandle = nullptr;
  std::unique_ptr<tgfx::Recording> lastRecording = nullptr;
  int lastDrawIndex = 0;
  std::shared_ptr<AppHost> appHost = nullptr;
  std::shared_ptr<tgfx::Window> tgfxWindow = nullptr;
  std::shared_ptr<tgfx::Surface> surface = nullptr;

  static WNDCLASS RegisterWindowClass();
  static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

  LRESULT handleMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

  void destroy();
  void centerAndShow();
  float getPixelRatio();
  void createAppHost();

  // Backend-specific. Implemented separately in win/d3d/TGFXWindow.cpp or
  // win/opengl/TGFXWindow.cpp.
  std::shared_ptr<tgfx::Window> createTGFXWindow(HWND hwnd);
  void draw();
};
}  // namespace benchmark
