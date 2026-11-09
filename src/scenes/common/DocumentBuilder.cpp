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

#include "scenes/common/DocumentBuilder.h"
#include "core/Random.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"

namespace benchmark {

bool BuildDocument(tgfx::Layer* root, const SceneContext& sceneContext, int64_t layerCount,
                   uint32_t seed, float documentWidth, float documentHeight,
                   std::vector<std::shared_ptr<tgfx::Layer>>* leaves) {
  if (root == nullptr || layerCount <= 0) {
    return false;
  }
  Random random(seed);
  const auto density = sceneContext.density;
  const auto groupCount = static_cast<int64_t>(64);
  const auto columns = static_cast<int64_t>(8);
  const auto cellWidth = documentWidth / static_cast<float>(columns);
  const auto cellHeight = documentHeight / static_cast<float>(columns);
  const auto perGroup = layerCount / groupCount + 1;

  for (int64_t groupIndex = 0; groupIndex < groupCount; groupIndex++) {
    auto group = tgfx::Layer::Make();
    if (group == nullptr) {
      return false;
    }
    auto column = groupIndex % columns;
    auto row = groupIndex / columns;
    group->setPosition(tgfx::Point{static_cast<float>(column) * cellWidth,
                                   static_cast<float>(row) * cellHeight});
    for (int64_t i = 0; i < perGroup; i++) {
      auto shape = tgfx::ShapeLayer::Make();
      tgfx::Path path;
      auto size = random.range(8.0f, 48.0f) * density;
      auto x = random.range(0.0f, cellWidth - size);
      auto y = random.range(0.0f, cellHeight - size);
      path.addRoundRect(tgfx::Rect::MakeXYWH(x, y, size, size), 4.0f * density, 4.0f * density);
      shape->setPath(path);
      shape->setFillStyle(tgfx::ShapeStyle::Make(
          tgfx::Color{random.nextFloat(), random.nextFloat(), random.nextFloat(), 0.9f}));
      group->addChild(shape);
      if (leaves != nullptr) {
        leaves->push_back(shape);
      }
    }
    root->addChild(group);
  }
  return true;
}

}  // namespace benchmark
