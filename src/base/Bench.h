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

struct DrawParam {
  size_t startCount = 1;
  size_t stepCount = 600;
  float minFPS = 60.0f;
  size_t maxCount = 1000000;
};

struct PerfData {
  float fps = 0.0f;
  float drawTime = 0.0f;
  size_t drawCount = 0;
};

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

  explicit Bench(std::string name);

  virtual ~Bench() = default;

  std::string name() const {
    return _name;
  }

  /**
   * Draws the contents to the given canvas.
   */
  void draw(tgfx::Canvas* canvas, const AppHost* host);

  /**
   * Returns true if the draw count has reached its maximum.
   */
  virtual bool isMaxDrawCountReached() const {
    return false;
  }

  /**
   * Returns the most recent draw count produced by the benchmark, or 0 if not applicable.
   */
  virtual size_t currentDrawCount() const {
    return 0;
  }

  /**
   * Returns the most recently sampled FPS, or 0 if not applicable.
   */
  virtual float currentFPS() const {
    return 0.f;
  }

  /**
   * Returns the current performance data.
   */
  virtual PerfData getPerfData() const {
    return {};
  }

 protected:
  virtual void onDraw(tgfx::Canvas* canvas, const AppHost* host) = 0;

 private:
  std::string _name;
};
}  // namespace benchmark