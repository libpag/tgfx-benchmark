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

#import "TGFXWindow.h"
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CADisplayLink.h>
#include <cmath>
#include <deque>
#include <filesystem>
#include "base/AppHost.h"
#include "base/Bench.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/metal/MetalWindow.h"

@implementation TGFXWindow {
  NSWindow* window;
  MTKView* view;
  std::shared_ptr<tgfx::Window> tgfxWindow;
  std::unique_ptr<benchmark::AppHost> appHost;
  std::deque<std::unique_ptr<tgfx::Recording>> pendingRecordings;
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
  view = [[MTKView alloc] initWithFrame:frame device:MTLCreateSystemDefaultDevice()];
  view.paused = YES;
  view.enableSetNeedsDisplay = NO;
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [view addGestureRecognizer:[[NSClickGestureRecognizer alloc]
                                 initWithTarget:self
                                         action:@selector(handleClick:)]];
  [window setContentView:view];
  [window center];
  [window makeKeyAndOrderFront:nil];
  [self updateSize];
  drawIndex = 0;
  if (@available(macOS 14, *)) {
    CADisplayLink* caDisplayLink = [view displayLinkWithTarget:self selector:@selector(redraw)];
    [caDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];
  } else {
    [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0
                                    target:self
                                  selector:@selector(redraw)
                                  userInfo:nil
                                   repeats:YES];
  }
}

- (void)handleClick:(NSClickGestureRecognizer*)gestureRecognizer {
  if (appHost != nullptr) {
    appHost->resetFrames();
  }
  drawIndex++;
}

- (void)mouseMoved:(NSEvent*)event {
  NSPoint location = [view convertPoint:[event locationInWindow] fromView:nil];
  location = [view convertPointToBacking:location];
  if (appHost != nullptr) {
    auto mouseX = static_cast<float>(location.x);
    auto mouseY = static_cast<float>(appHost->height()) - static_cast<float>(location.y);
    appHost->mouseMoved(mouseX, mouseY);
  }
}

- (void)mouseEntered:(NSEvent*)event {
  [self mouseMoved:event];
}

- (void)mouseExited:(NSEvent*)event {
  if (appHost != nullptr) {
    appHost->mouseMoved(-1, -1);
  }
}

- (void)updateSize {
  CGSize size = [view convertSizeToBacking:view.bounds.size];
  auto width = static_cast<int>(round(size.width));
  auto height = static_cast<int>(round(size.height));
  if (appHost == nullptr) {
    appHost = std::make_unique<benchmark::AppHost>();
    std::filesystem::path filePath = __FILE__;
    auto rootPath = filePath.parent_path().parent_path().parent_path().parent_path().parent_path().string();
    auto imagePath = rootPath + R"(/resources/assets/bridge.jpg)";
    auto image = tgfx::Image::MakeFromFile(imagePath);
    appHost->addImage("bridge", image);
    auto typeface = tgfx::Typeface::MakeFromName("PingFang SC", "");
    appHost->addTypeface("default", typeface);
    typeface = tgfx::Typeface::MakeFromName("Apple Color Emoji", "");
    appHost->addTypeface("emoji", typeface);
  } else {
    appHost->resetFrames();
  }
  auto contentScale = static_cast<float>(size.height / view.bounds.size.height);
  appHost->updateScreen(width, height, contentScale);
  for (NSTrackingArea* trackingArea in [view trackingAreas]) {
    [view removeTrackingArea:trackingArea];
  }
  NSTrackingArea* trackingArea = [[NSTrackingArea alloc]
      initWithRect:[view bounds]
           options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways)
             owner:self
          userInfo:nil];
  [trackingArea autorelease];
  [view addTrackingArea:trackingArea];
}

- (void)redraw {
  auto currentTime = tgfx::Clock::Now();
  if (appHost->width() <= 0 || appHost->height() <= 0) {
    return;
  }
  if (tgfxWindow == nullptr) {
    tgfxWindow = tgfx::MetalWindow::MakeFrom(view);
  }
  if (tgfxWindow == nullptr) {
    return;
  }
  auto device = tgfxWindow->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  auto surface = tgfx::Surface::MakeFrom(context, tgfxWindow);
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->clear({0.87f, 0.87f, 0.87f, 1.0f});
  auto numBenches = benchmark::Bench::Count();
  auto index = (drawIndex % numBenches);
  auto bench = benchmark::Bench::GetByIndex(index);
  bench->draw(canvas, appHost.get());
  auto recording = context->flush();
  if (recording != nullptr) {
    pendingRecordings.push_back(std::move(recording));
  }
  // Triple buffering: keep 2 recordings in flight, submit the oldest.
  while (pendingRecordings.size() > 2) {
    context->submit(std::move(pendingRecordings.front()));
    pendingRecordings.pop_front();
  }
  device->unlock();
  auto drawTime = tgfx::Clock::Now() - currentTime;
  appHost->recordFrame(drawTime);
}
@end
