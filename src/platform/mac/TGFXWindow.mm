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
#import <CoreVideo/CoreVideo.h>
#include <cmath>
#include <filesystem>
#include "base/AppHost.h"
#include "base/Bench.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Window.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@implementation TGFXWindow {
  NSWindow* window;
  NSView* view;
  std::shared_ptr<tgfx::Window> tgfxWindow;
  std::shared_ptr<tgfx::Surface> surface;
  std::unique_ptr<benchmark::AppHost> appHost;
  std::unique_ptr<tgfx::Recording> lastRecording;
  int drawIndex;
  bool closing;
  CVDisplayLinkRef displayLink;
}

- (void)stopDisplayLink {
  if (displayLink != nil) {
    CVDisplayLinkStop(displayLink);
    CVDisplayLinkSetOutputCallback(displayLink, nullptr, nullptr);
    CVDisplayLinkRelease(displayLink);
    displayLink = nil;
  }
}

- (void)dealloc {
  [self stopDisplayLink];
  lastRecording = nullptr;
  surface = nullptr;
  tgfxWindow = nullptr;
  [window release];
  [view release];
  [super dealloc];
}

- (void)windowWillClose:(NSNotification*)notification {
  closing = true;
  // Let AppKit finish the close notification before stopping the Core Video callback thread and
  // terminating the application. Already queued redraw blocks are ignored once closing is true.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self stopDisplayLink];
    [NSApp terminate:self];
  });
}

- (void)windowDidResize:(NSNotification*)notification {
  [self updateSize];
}

- (BOOL)isActiveDisplayLink:(CVDisplayLinkRef)link {
  return !closing && displayLink == link;
}

static CVReturn displayLinkCallback(CVDisplayLinkRef displayLink, const CVTimeStamp*,
                                    const CVTimeStamp*, CVOptionFlags, CVOptionFlags*,
                                    void* context) {
  auto self = (TGFXWindow*)context;
  dispatch_async(dispatch_get_main_queue(), ^{
    // The display link may have been stopped/released between scheduling and running this block.
    // Only redraw while the link that scheduled us is still the active one.
    if ([self isActiveDisplayLink:displayLink]) {
      [self redraw];
    }
  });
  return kCVReturnSuccess;
}

- (void)open {
  NSRect frame = NSMakeRect(0, 0, 1024, 720);
  NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
  window = [[NSWindow alloc] initWithContentRect:frame
                                       styleMask:styleMask
                                         backing:NSBackingStoreBuffered
                                           defer:NO];
  [window setReleasedWhenClosed:NO];
  [window setTitle:[TGFXWindow BackendTitle]];
  [window setDelegate:self];
  view = [TGFXWindow MakeBackendView:frame];
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  auto clickRecognizer = [[NSClickGestureRecognizer alloc] initWithTarget:self
                                                                   action:@selector(handleClick:)];
  [view addGestureRecognizer:clickRecognizer];
  [clickRecognizer release];
  [window setContentView:view];
  [window center];
  [window makeKeyAndOrderFront:nil];
  [self updateSize];
  drawIndex = 0;
  closing = false;
  displayLink = nil;
  auto result = CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
  if (result == kCVReturnSuccess && displayLink != nil) {
    result = CVDisplayLinkSetOutputCallback(displayLink, &displayLinkCallback, self);
  }
  if (result == kCVReturnSuccess && displayLink != nil) {
    result = CVDisplayLinkStart(displayLink);
  }
  if (result != kCVReturnSuccess) {
    [self stopDisplayLink];
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
    auto rootPath = filePath.parent_path().parent_path().parent_path().parent_path().string();
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
  if (width > 0 && height > 0 && view.bounds.size.height > 0) {
    auto contentScale = static_cast<float>(size.height / view.bounds.size.height);
    if (appHost->updateScreen(width, height, contentScale)) {
      surface = nullptr;
    }
  }
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
    tgfxWindow = [TGFXWindow MakeTGFXWindow:view];
  }
  if (tgfxWindow == nullptr) {
    return;
  }
  auto device = tgfxWindow->getDevice();
  if (device == nullptr) {
    return;
  }
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  if (surface == nullptr) {
    if (lastRecording != nullptr) {
      context->submit(std::move(lastRecording));
    }
    surface = tgfx::Surface::MakeFrom(context, tgfxWindow);
  }
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
  std::swap(lastRecording, recording);
  if (recording != nullptr) {
    context->submit(std::move(recording));
  }
  device->unlock();
  auto drawTime = tgfx::Clock::Now() - currentTime;
  appHost->recordFrame(drawTime);
}
@end

#pragma clang diagnostic pop