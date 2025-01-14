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

#import "AppDelegate.h"
#import "TGFXView.h"
#import "WindowDelegate.h"

@interface AppDelegate ()
@property(strong, nonatomic) NSWindow* window;
@property(strong, nonatomic) TGFXView* tgfxView;
@property(strong, nonatomic) WindowDelegate* windowDelegate;
@end

@implementation AppDelegate

- (void)dealloc {
  [_window release];
  [_windowDelegate release];
  [_tgfxView release];
  [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
  NSRect frame = NSMakeRect(0, 0, 1024, 720);
  NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
  self.window = [[NSWindow alloc] initWithContentRect:frame
                                            styleMask:styleMask
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
  [self.window setTitle:@"TGFX Benchmark"];
  self.windowDelegate = [[WindowDelegate alloc] init];
  [self.window setDelegate:self.windowDelegate];
  self.tgfxView = [[TGFXView alloc] initWithFrame:frame];
  [self.tgfxView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [self.window setContentView:self.tgfxView];
  [self.window center];
  [self.window makeKeyAndOrderFront:nil];
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
  // Insert code here to tear down your application
}

@end
