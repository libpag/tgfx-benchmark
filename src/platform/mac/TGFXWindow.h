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

#import <Cocoa/Cocoa.h>
#include <memory>

namespace tgfx {
class Window;
}

@interface TGFXWindow : NSObject <NSWindowDelegate>

- (void)open;

@end

// Backend-specific factory methods. Each is implemented in its own per-backend source file
// (MetalWindow.mm / CGLWindow.mm ...), selected by CMake according to BENCHMARK_BACKEND.
@interface TGFXWindow (Backend)

+ (NSString*)BackendTitle;
+ (NSView*)MakeBackendView:(NSRect)frame;
+ (std::shared_ptr<tgfx::Window>)MakeTGFXWindow:(NSView*)view;

@end
