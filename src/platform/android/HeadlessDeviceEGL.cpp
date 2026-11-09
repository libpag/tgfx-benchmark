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

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "core/HeadlessDevice.h"
#include "tgfx/gpu/opengl/egl/EGLDevice.h"
#include "tgfx/platform/Print.h"

namespace benchmark {
namespace {

constexpr EGLint kConfigAttributes[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
    EGL_RED_SIZE,        8,
    EGL_GREEN_SIZE,      8,
    EGL_BLUE_SIZE,       8,
    EGL_ALPHA_SIZE,      8,
    EGL_DEPTH_SIZE,      0,
    EGL_STENCIL_SIZE,    0,
    EGL_NONE,
};

constexpr EGLint kContextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

constexpr EGLint kSurfaceAttributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};

}  // namespace

std::shared_ptr<tgfx::Device> CreateHeadlessDevice() {
  // A pbuffer keeps the measurement independent of any window or surface the app happens to own.
  auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) != EGL_TRUE) {
    tgfx::PrintError("CreateHeadlessDevice: failed to initialize EGL");
    return nullptr;
  }
  EGLint configCount = 0;
  EGLConfig config = nullptr;
  if (eglChooseConfig(display, kConfigAttributes, &config, 1, &configCount) != EGL_TRUE ||
      configCount == 0) {
    tgfx::PrintError("CreateHeadlessDevice: no ES3 pbuffer config");
    return nullptr;
  }
  auto surface = eglCreatePbufferSurface(display, config, kSurfaceAttributes);
  if (surface == EGL_NO_SURFACE) {
    tgfx::PrintError("CreateHeadlessDevice: failed to create the pbuffer surface");
    return nullptr;
  }
  auto context = eglCreateContext(display, config, EGL_NO_CONTEXT, kContextAttributes);
  if (context == EGL_NO_CONTEXT) {
    tgfx::PrintError("CreateHeadlessDevice: failed to create the EGL context");
    eglDestroySurface(display, surface);
    return nullptr;
  }
  if (eglMakeCurrent(display, surface, surface, context) != EGL_TRUE) {
    tgfx::PrintError("CreateHeadlessDevice: failed to make the context current");
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    return nullptr;
  }
  return tgfx::EGLDevice::MakeFrom(display, surface, context, false);
}

}  // namespace benchmark
