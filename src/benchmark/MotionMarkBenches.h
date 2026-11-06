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

#include <vector>
#include "../base/Bench.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/Path.h"

namespace benchmark {

struct MultiplyTile {
  float normalizedX = 0.5f;
  float normalizedY = 0.5f;
  float rotation = 0.0f;
  float rotationSpeed = 0.0f;
  float radiusScale = 0.0f;
  float distance = 0.0f;
  size_t shape = 0;
};

class MultiplyBench : public Bench {
 public:
  MultiplyBench();
  void setCount(size_t count) override;
  size_t getCount() const override {
    return count;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void ensureTileCapacity(size_t count);

  size_t count = 512;
  std::vector<MultiplyTile> tiles;
};

struct MotionMarkArc {
  float normalizedX = 0.0f;
  float normalizedY = 0.0f;
  float radiusScale = 0.0f;
  float startAngle = 0.0f;
  float baseSweep = 0.0f;
  float omega = 0.0f;
  float width = 1.0f;
  bool counterclockwise = false;
  bool stroked = true;
  tgfx::Color color = tgfx::Color::Black();
};

class CanvasArcsBench : public Bench {
 public:
  CanvasArcsBench();
  void setCount(size_t count) override;
  size_t getCount() const override {
    return count;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void ensureArcCapacity(size_t count);

  size_t count = 8;
  std::vector<MotionMarkArc> arcs;
};

struct MotionMarkLeaf {
  float x = 0.0f;
  float y = 0.0f;
  float size = 0.0f;
  float angle = 0.0f;
  float angularSpeed = 0.0f;
  float velocityX = 0.0f;
  float velocityY = 0.0f;
  size_t imageIndex = 0;
};

class LeavesBench : public Bench {
 public:
  LeavesBench();
  void setCount(size_t count) override;
  size_t getCount() const override {
    return count;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void ensureParticleCapacity(size_t count, float width, float height);
  void resetParticle(MotionMarkLeaf& leaf, size_t index, float width, float height) const;

  size_t count = 128;
  float canvasWidth = 0.0f;
  float canvasHeight = 0.0f;
  std::vector<MotionMarkLeaf> particles;
};

struct MotionMarkLine {
  size_t circle = 0;
  float theta = 0.0f;
  float width = 1.0f;
  float omega = 0.0f;
  float length = 0.0f;
  float direction = 1.0f;
  float cosTheta = 1.0f;
  float sinTheta = 0.0f;
  tgfx::Color color = tgfx::Color::Black();
};

class CanvasLinesBench : public Bench {
 public:
  CanvasLinesBench();
  void setCount(size_t count) override;
  size_t getCount() const override {
    return count;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void ensureLineCapacity(size_t count);

  size_t count = 16;
  std::vector<MotionMarkLine> lines;
};

struct MotionMarkImageTile {
  float normalizedX = 0.0f;
  float normalizedY = 0.0f;
  float sizeScale = 1.0f;
  float angle = 0.0f;
  float angularSpeed = 0.0f;
  float orbitX = 0.0f;
  float orbitY = 0.0f;
  size_t imageIndex = 0;
};

class ImagesBench : public Bench {
 public:
  ImagesBench();
  void setCount(size_t count) override;
  size_t getCount() const override {
    return count;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void ensureTileCapacity(size_t count);

  size_t count = 64;
  std::vector<MotionMarkImageTile> tiles;
};

class DesignBench : public Bench {
 public:
  DesignBench();
  void setCount(size_t count) override;
  size_t getCount() const override {
    return count;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  size_t count = 8;
  std::vector<std::string> labels;
  tgfx::Font measuredFont;
  std::vector<float> labelWidths;
};

struct MotionMarkSuit {
  float x = 0.0f;
  float y = 0.0f;
  float size = 0.0f;
  float rotation = 0.0f;
  float angularSpeed = 0.0f;
  float velocityX = 0.0f;
  float velocityY = 0.0f;
  size_t shape = 0;
  tgfx::Color color = tgfx::Color::Red();
};

class SuitsBench : public Bench {
 public:
  SuitsBench();
  void setCount(size_t count) override;
  size_t getCount() const override {
    return count;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void ensureParticleCapacity(size_t count, float width, float height);
  void resetParticle(MotionMarkSuit& suit, size_t index, float width, float height) const;

  size_t count = 24;
  float canvasWidth = 0.0f;
  float canvasHeight = 0.0f;
  std::vector<MotionMarkSuit> particles;
};

struct ShaderMixTile {
  float phase = 0.0f;
  float speed = 0.0f;
  float normalizedX = 0.5f;
  float normalizedY = 0.5f;
  float rotation = 0.0f;
  float sizeScale = 0.15f;
  size_t imageIndex = 0;
  size_t motif = 0;
  size_t layer = 0;
  tgfx::Color color = tgfx::Color::Black();
};

class ShaderMixBench : public Bench {
 public:
  ShaderMixBench();
  void setCount(size_t count) override;
  size_t getCount() const override {
    return count;
  }

 protected:
  void onDraw(tgfx::Canvas* canvas, const AppHost* host) override;

 private:
  void ensureTileCapacity(size_t count);

  size_t count = 256;
  std::vector<ShaderMixTile> tiles;
};

}  // namespace benchmark
