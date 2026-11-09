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
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#import "TGFXWindow.h"
#include <memory>
#include "base/AppHost.h"
#include "base/Bench.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Device.h"
#include "tgfx/gpu/Recording.h"
#include "tgfx/gpu/Window.h"
#include "tgfx/core/Typeface.h"

@implementation TGFXWindowView

+ (Class)layerClass {
  return [TGFXWindow BackingLayerClass];
}

@end

@interface BenchmarkViewController : UIViewController
@end

@implementation BenchmarkViewController {
  std::unique_ptr<benchmark::AppHost> appHost;
  std::shared_ptr<tgfx::Window> tgfxWindow;
  std::shared_ptr<tgfx::Surface> surface;
  std::unique_ptr<tgfx::Recording> lastRecording;
  CADisplayLink* displayLink;
  int drawIndex;
}

- (void)dealloc {
  [displayLink invalidate];
  lastRecording = nullptr;
  surface = nullptr;
  tgfxWindow = nullptr;
  [super dealloc];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorWithWhite:0.87 alpha:1.0];
  drawIndex = 0;
  displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
  [displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

  UITapGestureRecognizer* tap =
      [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleTap:)];
  [self.view addGestureRecognizer:tap];
  [tap release];

  [self updateSize];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(orientationChanged:)
                                               name:UIDeviceOrientationDidChangeNotification
                                             object:nil];
}

- (void)updateSize {
  auto scale = static_cast<float>(self.view.contentScaleFactor);
  auto size = self.view.bounds.size;
  auto width = static_cast<int>(size.width * scale);
  auto height = static_cast<int>(size.height * scale);
  if (appHost == nullptr) {
    appHost = std::make_unique<benchmark::AppHost>();
    auto typeface = tgfx::Typeface::MakeFromName("PingFang SC", "");
    appHost->addTypeface("default", typeface);
  } else {
    appHost->resetFrames();
  }
  if (width > 0 && height > 0 && self.view.bounds.size.height > 0) {
    if (appHost->updateScreen(width, height, scale)) {
      surface = nullptr;
    }
  }
}

- (void)orientationChanged:(NSNotification*)notification {
  (void)notification;
  [self updateSize];
}

- (void)handleTap:(UITapGestureRecognizer*)recognizer {
  (void)recognizer;
  if (appHost != nullptr) {
    appHost->resetFrames();
  }
  drawIndex++;
}

- (void)tick:(CADisplayLink*)link {
  (void)link;
  if (appHost == nullptr || appHost->width() <= 0 || appHost->height() <= 0) {
    return;
  }
  if (tgfxWindow == nullptr) {
    tgfxWindow = [TGFXWindow MakeTGFXWindow:self.view];
  }
  if (tgfxWindow == nullptr) {
    return;
  }
  auto device = tgfxWindow->getDevice();
  if (device == nullptr) {
    return;
  }
  auto currentTime = tgfx::Clock::Now();
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
  auto count = benchmark::Bench::Count();
  auto bench = benchmark::Bench::GetByIndex(drawIndex % count);
  bench->draw(canvas, appHost.get());
  auto recording = context->flush();
  std::swap(lastRecording, recording);
  if (recording != nullptr) {
    context->submit(std::move(recording));
  }
  device->unlock();
  appHost->recordFrame(tgfx::Clock::Now() - currentTime);
}

@end

@implementation TGFXWindow {
  UIWindow* window;
}

- (void)open {
  window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  auto controller = [[BenchmarkViewController alloc] init];
  window.rootViewController = controller;
  [window makeKeyAndVisible];
  [controller release];
}

- (void)dealloc {
  [window release];
  [super dealloc];
}

@end
