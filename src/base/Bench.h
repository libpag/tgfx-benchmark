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

#include "AppHost.h"
#include "tgfx/core/Canvas.h"

namespace benchmark {
class Bench {
 public:
  /**
   * Returns the number of drawers.
   */
  static int Count();

  /**
   * Returns the names of all drawers.
   */
  static const std::vector<std::string>& Names();

  /**
   * Returns the drawer with the given index.
   */
  static Bench* GetByIndex(int index);

  /**
   * Returns the drawer with the given name.
   */
  static Bench* GetByName(const std::string& name);

  /**
   * Returns the drawer with the given stable benchmark scene ID.
   */
  static Bench* GetByStableId(const std::string& stableId);

  /**
   * Resolves a stable scene ID or a display name.
   */
  static Bench* Resolve(const std::string& identifier);

  explicit Bench(std::string name);

  virtual ~Bench() = default;

  virtual void setCount(size_t) {
  }

  virtual size_t getCount() const {
    return 0;
  }

  /**
   * Draws one frame and advances the scene animation clock.
   */
  void draw(tgfx::Canvas* canvas, const AppHost* host);

  std::string name() const {
    return _name;
  }

 protected:
  uint64_t frame = 0;
  double animationFrame = 0;
  float animationDelta = 1.0f;

  virtual void onDraw(tgfx::Canvas* canvas, const AppHost* host) = 0;

 private:
  std::string _name;
  int64_t lastAnimationUs = 0;
};
}  // namespace benchmark
