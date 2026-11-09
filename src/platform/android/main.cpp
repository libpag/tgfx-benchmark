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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <android_native_app_glue.h>
#include <chrono>
#include <thread>
#include "TGFXWindow.h"

namespace {

struct AppState {
  benchmark::TGFXWindow window = {};
  int width = 0;
  int height = 0;
};

void OnAppCommand(android_app* app, int32_t command) {
  auto* state = static_cast<AppState*>(app->userData);
  if (state == nullptr) {
    return;
  }
  switch (command) {
    case APP_CMD_INIT_WINDOW:
      if (app->window != nullptr) {
        state->window.open(app->window);
        state->width = ANativeWindow_getWidth(app->window);
        state->height = ANativeWindow_getHeight(app->window);
        state->window.resize(state->width, state->height);
      }
      break;
    case APP_CMD_TERM_WINDOW:
      state->window.close();
      break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
      if (app->window != nullptr) {
        state->width = ANativeWindow_getWidth(app->window);
        state->height = ANativeWindow_getHeight(app->window);
        state->window.resize(state->width, state->height);
      }
      break;
    default:
      break;
  }
}

}  // namespace

void android_main(android_app* app) {
  AppState state = {};
  app->userData = &state;
  app->onAppCmd = OnAppCommand;

  const auto frameInterval = std::chrono::milliseconds(16);
  auto nextFrame = std::chrono::steady_clock::now();

  while (app->destroyRequested == 0) {
    int events = 0;
    android_poll_source* source = nullptr;
    while (ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
      if (source != nullptr) {
        source->process(app, source);
      }
      if (app->destroyRequested != 0) {
        break;
      }
    }
    if (app->destroyRequested != 0) {
      break;
    }
    state.window.draw();
    nextFrame += frameInterval;
    std::this_thread::sleep_until(nextFrame);
  }
  state.window.close();
}
