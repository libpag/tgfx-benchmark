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

#pragma once

#include <android/native_window.h>
#include <cstdint>
#include <memory>
#include "base/AppHost.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Recording.h"
#include "tgfx/gpu/Window.h"

namespace benchmark {

/// Owns the interactive rendering state for the Android host.
class TGFXWindow {
 public:
  TGFXWindow() = default;
  ~TGFXWindow();

  bool open(ANativeWindow* nativeWindow);

  void close();

  void draw();

  void resize(int width, int height);

 private:
  std::shared_ptr<tgfx::Window> _tgfxWindow = {};
  std::shared_ptr<tgfx::Surface> _surface = {};
  std::unique_ptr<benchmark::AppHost> _appHost = {};
  std::unique_ptr<tgfx::Recording> _lastRecording = {};
  int64_t _drawIndex = 0;
};

}  // namespace benchmark
