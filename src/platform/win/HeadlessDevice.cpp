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

#include "core/HeadlessDevice.h"
#include "tgfx/platform/Print.h"

namespace benchmark {

std::shared_ptr<tgfx::Device> CreateHeadlessDevice() {
  // The Windows hosts only implement the interactive window so far. WGL, EGL, Vulkan and D3D12
  // all support offscreen contexts, so this is a matter of wiring them up rather than a missing
  // capability of the backends.
  tgfx::PrintError("headless modes are not implemented on Windows yet");
  return nullptr;
}

}  // namespace benchmark
