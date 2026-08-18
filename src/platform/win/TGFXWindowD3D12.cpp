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

#include "TGFXWindowBackend.h"
#include "tgfx/gpu/d3d12/D3D12Device.h"
#include "tgfx/gpu/d3d12/D3D12Window.h"

namespace benchmark {
LPCWSTR GetBackendWindowTitle() {
  return L"TGFX Benchmark - D3D12";
}

std::shared_ptr<tgfx::Window> MakeTGFXWindow(HWND windowHandle) {
  auto device = tgfx::D3D12Device::Make();
  return device == nullptr ? nullptr : tgfx::D3D12Window::MakeFrom(windowHandle, device);
}
}  // namespace benchmark
