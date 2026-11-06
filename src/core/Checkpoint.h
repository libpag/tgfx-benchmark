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

#pragma once

#include <cstdint>
#include <string>
#include "core/Result.h"
#include "tgfx/core/Surface.h"

namespace benchmark {

/**
 * Captures a deterministic frame so two builds can be checked for rendering equivalence before
 * their timings are compared.
 */
class Checkpoint {
 public:
  /// The background is the color the driver cleared the surface with. A frame that only contains
  /// the background is treated as blank, which catches scenes that silently draw nothing.
  static CheckpointResult Capture(tgfx::Surface* surface, const std::string& directory,
                                  const std::string& caseId, int64_t frame,
                                  const tgfx::Color& background);

  /// Thumbnail dimensions written next to the hash.
  static constexpr int ThumbnailWidth = 64;
  static constexpr int ThumbnailHeight = 36;
};

}  // namespace benchmark
