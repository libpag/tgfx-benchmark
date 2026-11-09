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

#import "TGFXWindow.h"
#import <QuartzCore/CAMetalLayer.h>
#include "tgfx/gpu/metal/MetalWindow.h"

@implementation TGFXWindow (Backend)

+ (NSString*)BackendTitle {
  return @"TGFX Benchmark (Metal)";
}

+ (Class)BackingLayerClass {
  return [CAMetalLayer class];
}

+ (std::shared_ptr<tgfx::Window>)MakeTGFXWindow:(UIView*)view {
  auto layer = (CAMetalLayer*)view.layer;
  layer.contentsScale = view.contentScaleFactor;
  layer.framebufferOnly = NO;
  auto size = view.bounds.size;
  auto scale = view.contentScaleFactor;
  layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
  return tgfx::MetalWindow::MakeFrom(layer);
}

@end
