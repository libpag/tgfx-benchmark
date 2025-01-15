/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "TGFXWindow.h"
#import <QuartzCore/CADisplayLink.h>
#include <cmath>
#include <filesystem>
#include "base/AppHost.h"
#include "base/Bench.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/gpu/opengl/cgl/CGLWindow.h"

@implementation TGFXWindow {
  NSWindow* window;
  NSView* view;
  std::shared_ptr<tgfx::CGLWindow> cglWindow;
  std::unique_ptr<benchmark::AppHost> appHost;
  int drawIndex;
}

- (void)dealloc {
  [window release];
  [view release];
  [super dealloc];
}

- (void)windowWillClose:(NSNotification*)notification {
  [NSApp terminate:self];
}

- (void)windowDidResize:(NSNotification*)notification {
  [self updateSize];
}

- (void)open {
  NSRect frame = NSMakeRect(0, 0, 1024, 720);
  NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
  window = [[NSWindow alloc] initWithContentRect:frame
                                       styleMask:styleMask
                                         backing:NSBackingStoreBuffered
                                           defer:NO];
  [window setTitle:@"TGFX Benchmark"];
  [window setDelegate:self];
  view = [[NSView alloc] initWithFrame:frame];
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [view addGestureRecognizer:[[NSClickGestureRecognizer alloc]
                                 initWithTarget:self
                                         action:@selector(handleClick:)]];
  [window setContentView:view];
  [window center];
  [window makeKeyAndOrderFront:nil];
  [self updateSize];
  drawIndex = 0;
  CADisplayLink* displayLink = [view displayLinkWithTarget:self selector:@selector(redraw)];
  [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)handleClick:(NSClickGestureRecognizer*)gestureRecognizer {
  drawIndex++;
}

- (void)updateSize {
  CGSize size = [view convertSizeToBacking:view.bounds.size];
  auto width = static_cast<int>(round(size.width));
  auto height = static_cast<int>(round(size.height));
  if (appHost == nullptr) {
    appHost = std::make_unique<benchmark::AppHost>();
    std::filesystem::path filePath = __FILE__;
    auto rootPath = filePath.parent_path().parent_path().parent_path().parent_path().string();
    auto imagePath = rootPath + R"(/resources/assets/bridge.jpg)";
    auto image = tgfx::Image::MakeFromFile(imagePath);
    appHost->addImage("bridge", image);
    auto typeface = tgfx::Typeface::MakeFromName("PingFang SC", "");
    appHost->addTypeface("default", typeface);
    typeface = tgfx::Typeface::MakeFromName("Apple Color Emoji", "");
    appHost->addTypeface("emoji", typeface);
  }
  auto contentScale = static_cast<float>(size.height / view.bounds.size.height);
  auto sizeChanged = appHost->updateScreen(width, height, contentScale);
  if (sizeChanged && cglWindow != nullptr) {
    cglWindow->invalidSize();
  }
}

- (void)redraw {
  if (appHost->width() <= 0 || appHost->height() <= 0) {
    return;
  }
  if (cglWindow == nullptr) {
    cglWindow = tgfx::CGLWindow::MakeFrom(view);
  }
  if (cglWindow == nullptr) {
    return;
  }
  auto device = cglWindow->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  auto surface = cglWindow->getSurface(context);
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->clear();
  auto numBenches = benchmark::Bench::Count();
  auto index = (drawIndex % numBenches);
  auto bench = benchmark::Bench::GetByIndex(index);
  bench->draw(canvas, appHost.get());
  context->flushAndSubmit();
  cglWindow->present(context);
  device->unlock();
}
@end
