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
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#import <UIKit/UIKit.h>
#include <memory>

namespace tgfx {
class Window;
}

/// The view the benchmark renders into. Its backing layer class is provided by the backend.
@interface TGFXWindowView : UIView
@end

@interface TGFXWindow : NSObject

- (void)open;

@end

// Backend-specific factories, implemented once per backend in MetalWindow.mm / EAGLWindow.mm.
@interface TGFXWindow (Backend)

+ (NSString*)BackendTitle;
+ (Class)BackingLayerClass;
+ (std::shared_ptr<tgfx::Window>)MakeTGFXWindow:(UIView*)view;

@end
