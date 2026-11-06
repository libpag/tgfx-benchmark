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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "base/AppHost.h"
#include "base/Bench.h"
#include "base/NativeBenchmarkState.h"
#include "benchmark/ParticleBench.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/Window.h"
#if TGFX_BENCHMARK_HAS_AOT_CACHE
#include "gpu/GlobalCache.h"
#include "gpu/PrecompiledShaderCache.h"
#endif

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
  benchmark::NativeBenchmarkState benchmarkState;
  benchmark::Bench* selectedBench;
  uint64_t nextFrameId;
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
  selectedBench = nullptr;
  nextFrameId = 1;
  benchmarkState.configureFromEnvironment();
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
  const auto callbackBeginUs = tgfx::Clock::Now();
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
#if TGFX_BENCHMARK_HAS_AOT_CACHE
  static bool aotAuditInitialized = false;
  if (!aotAuditInitialized && std::getenv("TGFX_BENCH_AOT_AUDIT") != nullptr) {
    context->precompiledShaderCache()->setDiagnosticRecordingEnabled(true);
    aotAuditInitialized = true;
  }
#endif
  auto canvas = surface->getCanvas();
  canvas->clear({0.87f, 0.87f, 0.87f, 1.0f});
  if (selectedBench == nullptr) {
    const char* sceneEnv = std::getenv("TGFX_BENCH_SCENE");
    if (sceneEnv != nullptr) {
      selectedBench = benchmark::Bench::Resolve(sceneEnv);
      if (selectedBench == nullptr) {
        std::fprintf(stderr, "Unknown TGFX_BENCH_SCENE: %s\n", sceneEnv);
        std::exit(EXIT_FAILURE);
      }
    }
    if (selectedBench == nullptr) {
      selectedBench = benchmark::Bench::GetByIndex(drawIndex % benchmark::Bench::Count());
    }
    const char* countEnv = std::getenv("TGFX_BENCH_COUNT");
    if (countEnv != nullptr && selectedBench != nullptr) {
      selectedBench->setCount(static_cast<size_t>(std::strtoull(countEnv, nullptr, 10)));
    }
  }

  const auto sceneDrawStartUs = tgfx::Clock::Now();
  selectedBench->draw(canvas, appHost.get());
  const auto sceneDrawUs = tgfx::Clock::Now() - sceneDrawStartUs;

  const auto flushStartUs = tgfx::Clock::Now();
  auto recording = context->flush();
  const auto flushUs = tgfx::Clock::Now() - flushStartUs;
  std::swap(lastRecording, recording);
  int64_t submitUs = 0;
  if (recording != nullptr) {
    const auto submitStartUs = tgfx::Clock::Now();
    context->submit(std::move(recording));
    submitUs = tgfx::Clock::Now() - submitStartUs;
  }
  device->unlock();
  const auto callbackEndUs = tgfx::Clock::Now();
  appHost->recordFrame(callbackEndUs - callbackBeginUs);
  if (benchmarkState.enabled()) {
    if (nextFrameId == 1) {
      benchmarkState.start(callbackBeginUs);
    }
    benchmarkState.recordFrame(benchmark::NativeFrameSample{
        nextFrameId++, callbackBeginUs, callbackEndUs, sceneDrawUs, flushUs, submitUs});
  }

  // Machine-readable perf logging so automated A/B runs can capture per-sample curves
  // (auto-scaling ramp and fixed-count steady state) without reading the on-screen overlay.
  static int64_t startTime = 0;
  static int64_t lastLogTime = 0;
  static bool labelPrinted = false;
  static bool firstFrameLogged = false;
  auto now = tgfx::Clock::Now();
  if (startTime == 0) {
    startTime = now;
  }
  if (!labelPrinted) {
    const char* label = std::getenv("TGFX_BENCH_LABEL");
    printf("BENCH_LABEL %s\n", label == nullptr ? "unlabeled" : label);
    printf("BENCH_CONTEXT width=%d height=%d density=%.2f\n", appHost->width(),
           appHost->height(), static_cast<double>(appHost->density()));
    const char* backendName = "unknown";
    switch (context->backend()) {
      case tgfx::Backend::OpenGL:
        backendName = "opengl";
        break;
      case tgfx::Backend::Metal:
        backendName = "metal";
        break;
      case tgfx::Backend::Vulkan:
        backendName = "vulkan";
        break;
      case tgfx::Backend::WebGPU:
        backendName = "webgpu";
        break;
      case tgfx::Backend::D3D12:
        backendName = "d3d12";
        break;
      case tgfx::Backend::Unknown:
        break;
    }
    printf("BENCH_BACKEND backend=%s\n", backendName);
    fflush(stdout);
    labelPrinted = true;
  }
  if (!firstFrameLogged) {
    printf("BENCH_FIRSTFRAME time_ms=%.2f\n",
           static_cast<double>(callbackEndUs - callbackBeginUs) / 1000.0);
    fflush(stdout);
    firstFrameLogged = true;
  }
  if (lastLogTime == 0) {
    lastLogTime = now;
  }
  if (now - lastLogTime >= 500000) {
    auto* particleBench = dynamic_cast<benchmark::ParticleBench*>(selectedBench);
    if (particleBench != nullptr) {
      const auto perf = particleBench->getPerfData();
      printf("BENCH_SAMPLE t_s=%.3f fps=%.1f time_ms=%.2f count=%zu reached=%d\n",
             static_cast<double>(now - startTime) / 1000000.0, perf.fps,
             static_cast<double>(perf.drawTime), perf.drawCount,
             particleBench->isMaxDrawCountReached() ? 1 : 0);
    } else {
      printf("BENCH_SAMPLE t_s=%.3f\n", static_cast<double>(now - startTime) / 1000000.0);
    }
    fflush(stdout);
    lastLogTime = now;
  }
#if TGFX_BENCHMARK_HAS_AOT_CACHE
  const char* aotAuditPath = std::getenv("TGFX_BENCH_AOT_AUDIT");
  static bool aotAuditWritten = false;
  if (aotAuditPath != nullptr && !aotAuditWritten && now - startTime >= 2000000) {
    auto* cache = context->precompiledShaderCache();
    const auto drawStats = cache->drawStats();
    const auto programStats = context->globalCache()->programStats();
    printf("BENCH_AOT_AUDIT bundleLoaded=%d bundleGeneration=%llu draws=%llu "
           "completeAOTDraws=%llu precompiledPrograms=%llu programBuilderPrograms=%llu "
           "droppedDraws=%llu directChainDraws=%llu offscreenPlanDraws=%llu "
           "hits=%u misses=%u attempts=%u permutations=%u artifacts=%u pipelines=%u\n",
           cache->isLoaded() ? 1 : 0,
           static_cast<unsigned long long>(cache->bundleGeneration()),
           static_cast<unsigned long long>(drawStats.draws),
           static_cast<unsigned long long>(drawStats.completeAOTDraws),
           static_cast<unsigned long long>(programStats.precompiledArtifactCreations),
           static_cast<unsigned long long>(programStats.programBuilderCreations),
           static_cast<unsigned long long>(drawStats.droppedDraws),
           static_cast<unsigned long long>(drawStats.directChainDraws),
           static_cast<unsigned long long>(drawStats.offscreenPlanDraws), cache->hitCount(),
           cache->missCount(),
           cache->aotStageCount(tgfx::PrecompiledAOTStage::Attempt),
           cache->aotStageCount(tgfx::PrecompiledAOTStage::PermutationMatched),
           cache->aotStageCount(tgfx::PrecompiledAOTStage::ArtifactsFound),
           cache->aotStageCount(tgfx::PrecompiledAOTStage::PipelineCreated));
    cache->writeDiagnosticsDump(aotAuditPath);
    fflush(stdout);
    aotAuditWritten = true;
  }
#endif

  if (benchmarkState.enabled() && benchmarkState.shouldStop(callbackEndUs)) {
    printf("BENCH_JSON %s\n", benchmarkState.summaryJson().c_str());
    fflush(stdout);
    [NSApp terminate:self];
  }
}
@end

#pragma clang diagnostic pop
