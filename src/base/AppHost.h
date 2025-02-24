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

#pragma once

#include <deque>
#include <unordered_map>
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Typeface.h"

namespace benchmark {
/**
 * AppHost provides information about the current app context.
 */
class AppHost {
 public:
  /**
   * Creates an AppHost with the given width, height and density. The width and height are in
   * pixels, and the density is the ratio of physical pixels to logical pixels.
   */
  explicit AppHost(int width = 1280, int height = 720, float density = 1.0f);

  virtual ~AppHost() = default;

  /**
   * Returns the width of the screen.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of the screen.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the density of the screen.
   */
  float density() const {
    return _density;
  }

  /**
   * Returns the mouse x position.
   */
  float mouseX() const {
    return _mouseX;
  }

  /**
   * Returns the mouse y position.
   */
  float mouseY() const {
    return _mouseY;
  }

  /**
   * Returns the current frames per second. Returns 0 if the FPS is not available yet.
   */
  float currentFPS() const;

  /**
   * Returns the last draw time in microseconds.
   */
  int64_t lastDrawTime() const {
    return drawTimes.empty() ? 0 : drawTimes.back();
  }

  /**
   * Returns the average draw time in microseconds.
   */
  int64_t averageDrawTime() const;

  /**
   * Returns true if this is the first frame.
   */
  bool isFirstFrame() const {
    return fpsTimeStamps.empty();
  }

  /**
   * Returns an image with the given name.
   */
  std::shared_ptr<tgfx::Image> getImage(const std::string& name) const;

  /**
   * Returns a typeface with the given name.
   */
  std::shared_ptr<tgfx::Typeface> getTypeface(const std::string& name) const;

  /**
   * Updates the screen size and density. The default values are 1280x720 and 1.0. The width and
   * height are in pixels, and the density is the ratio of physical pixels to logical pixels.
   * Returns true if the screen size or density has changed.
   */
  bool updateScreen(int width, int height, float density);

  /**
   * Updates the mouse position.
   */
  void mouseMoved(float mouseX, float mouseY) {
    this->_mouseX = mouseX;
    this->_mouseY = mouseY;
  }

  /**
   * Add an image for the given resource name.
   */
  void addImage(const std::string& name, std::shared_ptr<tgfx::Image> image);

  /**
   * Adds a typeface for the given resource name.
   */
  void addTypeface(const std::string& name, std::shared_ptr<tgfx::Typeface> typeface);

  /**
   * Marks the end of a frame and records the frame time.
   */
  void recordFrame(int64_t drawTime);

  /**
   * Resets the app host to the first frame.
   */
  void resetFrames();

 private:
  int _width = 1024;
  int _height = 720;
  float _density = 1.0f;
  float _mouseX = -1.0f;
  float _mouseY = -1.0f;
  std::deque<int64_t> fpsTimeStamps = {};
  std::deque<int64_t> drawTimes = {};
  std::unordered_map<std::string, std::shared_ptr<tgfx::Image>> images = {};
  std::unordered_map<std::string, std::shared_ptr<tgfx::Typeface>> typefaces = {};
};
}  // namespace benchmark
