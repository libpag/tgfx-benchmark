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
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "core/Scene.h"

namespace tgfx {
class Layer;
}

namespace benchmark {

/**
 * Builds a design document style layer tree under the given root: a shallow group level with leaf
 * shape layers spread uniformly over the document area. Shared by the doc.* scenes so that pan and
 * zoom measure the same content.
 *
 * Returns false when the root is null or no layer could be added.
 */
bool BuildDocument(tgfx::Layer* root, const SceneContext& sceneContext, int64_t layerCount,
                   uint32_t seed, float documentWidth, float documentHeight);

}  // namespace benchmark
