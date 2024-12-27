/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx-benchmark available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "ViewController.h"
#import <QuartzCore/CVDisplayLink.h>
#import "TGFXView.h"

@interface ViewController ()
@property(strong, nonatomic) TGFXView* tgfxView;
@property(nonatomic) int drawCount;
@end

CVDisplayLinkRef displayLink;

static CVReturn OnAnimationCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*,
                                    CVOptionFlags, CVOptionFlags*, void* userInfo) {
  auto viewController = (__bridge ViewController*)userInfo;
  dispatch_queue_t mainQueue = dispatch_get_main_queue();
  // draw Frame in mainThread
  dispatch_async(mainQueue, ^{
    [viewController redraw];
  });
  return kCVReturnSuccess;
}

@implementation ViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tgfxView = [[TGFXView alloc] initWithFrame:self.view.bounds];
  [self.tgfxView setAutoresizingMask:kCALayerWidthSizable | kCALayerHeightSizable];
  [self.view addSubview:self.tgfxView];
  [self.tgfxView draw:self.drawCount];
  CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
  CVDisplayLinkSetOutputCallback(displayLink, &OnAnimationCallback, (__bridge void*)self);
}

- (void)viewDidLayout {
  [super viewDidLayout];
  [self.tgfxView draw:self.drawCount];
  CVDisplayLinkStart(displayLink);
}

- (void)mouseUp:(NSEvent*)event {
  self.drawCount++;
  [self.tgfxView draw:self.drawCount];
}

- (void)redraw {
  [self.tgfxView draw:self.drawCount];
}

@end
