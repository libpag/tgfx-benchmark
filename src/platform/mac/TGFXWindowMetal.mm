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

#import <MetalKit/MetalKit.h>
#import "TGFXWindowBackend.h"
#include "tgfx/gpu/metal/MetalWindow.h"

namespace benchmark {
NSString* GetBackendWindowTitle() {
  return @"TGFX Benchmark - Metal";
}

NSView* MakeBackendView(NSRect frame) {
  auto view = [[MTKView alloc] initWithFrame:frame];
  [view setPaused:YES];
  [view setEnableSetNeedsDisplay:NO];
  return view;
}

std::shared_ptr<tgfx::Window> MakeTGFXWindow(NSView* view) {
  return tgfx::MetalWindow::MakeFrom((MTKView*)view);
}
}  // namespace benchmark
