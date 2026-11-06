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
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "MotionMarkBenches.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include "tgfx/core/ColorFilter.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/MaskFilter.h"
#include "tgfx/core/Mesh.h"
#include "tgfx/core/SamplingOptions.h"
#include "tgfx/core/Shader.h"
#include "tgfx/core/TextBlob.h"

namespace benchmark {
namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr size_t kImageCount = 13;
constexpr size_t kShaderMixMotifCount = 13;
constexpr std::array<const char*, kImageCount> ImageNames = {
    "motionmark.compass",  "motionmark.console", "motionmark.contribute",
    "motionmark.debugger", "motionmark.inspector", "motionmark.layout",
    "motionmark.performance", "motionmark.script", "motionmark.shortcuts",
    "motionmark.standards", "motionmark.storage", "motionmark.styles",
    "motionmark.timeline"};
constexpr std::array<float, kShaderMixMotifCount> ShaderMixScales = {
    0.115f, 0.130f, 0.160f, 0.140f, 0.150f, 0.170f, 0.145f,
    0.140f, 0.140f, 0.170f, 0.155f, 0.170f, 0.130f,
};

tgfx::Color ShaderMixPaletteColor(size_t index) {
  static const std::array<tgfx::Color, 6> palette = {
      tgfx::Color{0.20f, 0.82f, 0.90f, 0.94f},
      tgfx::Color{0.98f, 0.62f, 0.30f, 0.94f},
      tgfx::Color{0.94f, 0.40f, 0.48f, 0.94f},
      tgfx::Color{0.52f, 0.78f, 0.48f, 0.94f},
      tgfx::Color{0.74f, 0.62f, 0.96f, 0.94f},
      tgfx::Color{0.94f, 0.89f, 0.72f, 0.94f},
  };
  return palette[index % palette.size()];
}

size_t ShaderMixLayer(size_t motif) {
  switch (motif) {
    case 2:
    case 4:
    case 9:
      return 0;
    case 3:
    case 6:
    case 10:
      return 1;
    case 0:
    case 7:
    case 11:
      return 2;
    default:
      return 3;
  }
}

float Fraction(float value) {
  return value - std::floor(value);
}

void ReflectParticle(float& position, float& velocity, float lower, float upper) {
  if (position < lower) {
    position = std::min(upper, lower + (lower - position));
    velocity = std::abs(velocity);
  } else if (position > upper) {
    position = std::max(lower, upper - (position - upper));
    velocity = -std::abs(velocity);
  }
}

float Unit(size_t index, size_t salt) {
  const auto value = std::sin(static_cast<float>(index) * 127.1f +
                              static_cast<float>(salt) * 311.7f) *
                     43758.5453f;
  return value - std::floor(value);
}

float Lerp(float start, float end, float progress) {
  return start + (end - start) * progress;
}

tgfx::Color Hsl(float hueDegrees, float saturation, float lightness, float alpha = 1.0f) {
  hueDegrees = std::fmod(hueDegrees, 360.0f);
  if (hueDegrees < 0.0f) {
    hueDegrees += 360.0f;
  }
  const auto chroma = (1.0f - std::abs(2.0f * lightness - 1.0f)) * saturation;
  const auto segment = hueDegrees / 60.0f;
  const auto x = chroma * (1.0f - std::abs(std::fmod(segment, 2.0f) - 1.0f));
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  if (segment < 1.0f) {
    red = chroma;
    green = x;
  } else if (segment < 2.0f) {
    red = x;
    green = chroma;
  } else if (segment < 3.0f) {
    green = chroma;
    blue = x;
  } else if (segment < 4.0f) {
    green = x;
    blue = chroma;
  } else if (segment < 5.0f) {
    red = x;
    blue = chroma;
  } else {
    red = chroma;
    blue = x;
  }
  const auto match = lightness - chroma * 0.5f;
  return {red + match, green + match, blue + match, alpha};
}

tgfx::Paint Fill(tgfx::Color color) {
  tgfx::Paint paint;
  paint.setColor(color);
  paint.setAntiAlias(true);
  return paint;
}

tgfx::Paint Stroke(tgfx::Color color, float width) {
  tgfx::Paint paint;
  paint.setColor(color);
  paint.setAntiAlias(true);
  paint.setStyle(tgfx::PaintStyle::Stroke);
  paint.setStrokeWidth(width);
  paint.setLineCap(tgfx::LineCap::Butt);
  paint.setLineJoin(tgfx::LineJoin::Bevel);
  return paint;
}

void DrawShaderMixBackdrop(tgfx::Canvas* canvas, float width, float height, float phase) {
  auto field = Fill(tgfx::Color::White());
  field.setShader(tgfx::Shader::MakeLinearGradient(
      {0.0f, 0.0f}, {width, height},
      {{0.060f, 0.075f, 0.085f, 1.0f},
       {0.090f, 0.115f, 0.125f, 1.0f},
       {0.070f, 0.080f, 0.105f, 1.0f}}));
  canvas->drawRect(tgfx::Rect::MakeWH(width, height), field);

  const auto minSize = std::min(width, height);
  const auto orbit = Stroke({0.70f, 0.90f, 0.96f, 0.09f}, std::max(1.0f, minSize * 0.0015f));
  for (size_t index = 0; index < 3; ++index) {
    const auto offset = static_cast<float>(index) * 0.25f;
    const auto wave = std::sin(phase * 0.18f + static_cast<float>(index)) * height * 0.035f;
    tgfx::Path flow;
    flow.moveTo(0.0f, height * (0.28f + offset) + wave);
    flow.cubicTo(width * 0.30f, height * (0.02f + offset),
                  width * 0.62f, height * (0.64f + offset),
                  width, height * (0.30f + offset) - wave);
    canvas->drawPath(flow, orbit);
  }

  auto ribbon = Stroke(tgfx::Color::White(), std::max(1.25f, minSize * 0.003f));
  ribbon.setShader(tgfx::Shader::MakeLinearGradient(
      {0.0f, height * 0.2f}, {width, height * 0.8f},
      {{0.20f, 0.82f, 0.90f, 0.0f},
       {0.94f, 0.89f, 0.72f, 0.34f},
       {0.94f, 0.40f, 0.48f, 0.0f}}));
  tgfx::Path sweep;
  sweep.moveTo(-width * 0.08f, height * (0.70f + 0.03f * std::sin(phase * 0.2f)));
  sweep.cubicTo(width * 0.24f, height * 0.18f, width * 0.62f,
                height * (0.90f + 0.03f * std::cos(phase * 0.18f)),
                width * 1.08f, height * 0.30f);
  canvas->drawPath(sweep, ribbon);
}

std::shared_ptr<tgfx::Image> MotionMarkImage(const AppHost* host, size_t index) {
  return host->getImage(ImageNames[index % ImageNames.size()]);
}

tgfx::Rect ImageSource(const std::shared_ptr<tgfx::Image>& image) {
  return image == nullptr
      ? tgfx::Rect::MakeEmpty()
      : tgfx::Rect::MakeWH(static_cast<float>(image->width()),
                           static_cast<float>(image->height()));
}

tgfx::Path SuitPath(size_t shape, const tgfx::Rect& bounds) {
  tgfx::Path path;
  switch (shape % 4) {
    case 0:
      path.moveTo(0.5f, 0.214f);
      path.cubicTo(0.466f, 0.164f, 0.369f, 0.09f, 0.267f, 0.092f);
      path.cubicTo(0.0f, 0.093f, 0.0f, 0.186f, 0.0f, 0.345f);
      path.cubicTo(0.0f, 0.659f, 0.395f, 0.655f, 0.5f, 0.938f);
      path.cubicTo(0.605f, 0.655f, 1.0f, 0.659f, 1.0f, 0.345f);
      path.cubicTo(1.0f, 0.186f, 1.0f, 0.093f, 0.733f, 0.092f);
      path.cubicTo(0.631f, 0.09f, 0.534f, 0.164f, 0.5f, 0.214f);
      break;
    case 1:
      path.moveTo(0.5f, 0.018f);
      path.cubicTo(0.62f, 0.018f, 0.718f, 0.115f, 0.718f, 0.235f);
      path.cubicTo(0.718f, 0.31f, 0.679f, 0.377f, 0.618f, 0.418f);
      path.cubicTo(0.703f, 0.382f, 0.78f, 0.381f, 0.86f, 0.42f);
      path.cubicTo(0.967f, 0.472f, 0.967f, 0.69f, 0.749f, 0.806f);
      path.cubicTo(0.664f, 0.805f, 0.578f, 0.756f, 0.542f, 0.677f);
      path.cubicTo(0.538f, 0.799f, 0.605f, 0.918f, 0.708f, 0.982f);
      path.cubicTo(0.593f, 0.941f, 0.407f, 0.941f, 0.292f, 0.982f);
      path.cubicTo(0.397f, 0.917f, 0.461f, 0.799f, 0.459f, 0.676f);
      path.cubicTo(0.422f, 0.756f, 0.337f, 0.804f, 0.251f, 0.806f);
      path.cubicTo(0.033f, 0.69f, 0.033f, 0.472f, 0.14f, 0.42f);
      path.cubicTo(0.22f, 0.381f, 0.298f, 0.38f, 0.382f, 0.418f);
      path.cubicTo(0.319f, 0.378f, 0.284f, 0.309f, 0.282f, 0.235f);
      path.cubicTo(0.282f, 0.115f, 0.38f, 0.018f, 0.5f, 0.018f);
      break;
    case 2:
      path.moveTo(0.301f, 0.982f);
      path.cubicTo(0.374f, 0.941f, 0.469f, 0.804f, 0.469f, 0.72f);
      path.cubicTo(0.374f, 0.857f, 0.039f, 0.825f, 0.049f, 0.563f);
      path.cubicTo(0.059f, 0.28f, 0.406f, 0.269f, 0.5f, 0.018f);
      path.cubicTo(0.594f, 0.269f, 0.941f, 0.28f, 0.951f, 0.563f);
      path.cubicTo(0.961f, 0.825f, 0.626f, 0.857f, 0.531f, 0.72f);
      path.cubicTo(0.531f, 0.804f, 0.626f, 0.941f, 0.699f, 0.982f);
      path.cubicTo(0.584f, 0.941f, 0.416f, 0.941f, 0.301f, 0.982f);
      break;
    default:
      path.moveTo(0.495f, 0.0f);
      path.cubicTo(0.424f, 0.153f, 0.199f, 0.439f, 0.128f, 0.5f);
      path.cubicTo(0.199f, 0.561f, 0.424f, 0.847f, 0.495f, 1.0f);
      path.cubicTo(0.566f, 0.847f, 0.791f, 0.561f, 0.862f, 0.5f);
      path.cubicTo(0.791f, 0.439f, 0.566f, 0.153f, 0.495f, 0.0f);
      break;
  }
  path.close();
  path.transform(tgfx::Matrix::MakeScale(bounds.width(), bounds.height()));
  path.transform(tgfx::Matrix::MakeTrans(bounds.left, bounds.top));
  return path;
}

const std::shared_ptr<tgfx::Mesh>& GetShaderMixMesh() {
  static const std::shared_ptr<tgfx::Mesh> mesh = [] {
    const tgfx::Point positions[] = {
        {0.0f, 0.0f}, {0.52f, 0.0f}, {1.0f, 0.42f},
        {0.0f, 0.0f}, {1.0f, 0.42f}, {0.38f, 1.0f},
    };
    const tgfx::Color colors[] = {
        {0.96f, 0.28f, 0.30f, 1.0f}, {0.98f, 0.76f, 0.20f, 1.0f},
        {0.20f, 0.88f, 0.62f, 1.0f}, {0.20f, 0.86f, 0.94f, 1.0f},
        {0.95f, 0.35f, 0.68f, 1.0f}, {0.50f, 0.42f, 0.98f, 1.0f},
    };
    return tgfx::Mesh::MakeCopy(tgfx::MeshTopology::Triangles, 6, positions, nullptr, colors);
  }();
  return mesh;
}

const std::shared_ptr<tgfx::ColorFilter>& GetShaderMixColorFilter() {
  static const std::shared_ptr<tgfx::ColorFilter> filter = [] {
    const std::array<float, 20> matrix = {
        0.55f, 0.25f, 0.20f, 0.0f, 0.08f,
        0.20f, 0.75f, 0.05f, 0.0f, 0.06f,
        0.10f, 0.20f, 0.70f, 0.0f, 0.12f,
        0.00f, 0.00f, 0.00f, 1.0f, 0.00f,
    };
    return tgfx::ColorFilter::Compose(
        tgfx::ColorFilter::Matrix(matrix),
        tgfx::ColorFilter::Luma());
  }();
  return filter;
}

const std::shared_ptr<tgfx::ImageFilter>& GetShaderMixBlurFilter() {
  static const std::shared_ptr<tgfx::ImageFilter> filter =
      tgfx::ImageFilter::Blur(1.5f, 1.0f, tgfx::TileMode::Clamp);
  return filter;
}

const std::shared_ptr<tgfx::ImageFilter>& GetShaderMixShadowFilter() {
  static const std::shared_ptr<tgfx::ImageFilter> filter =
      tgfx::ImageFilter::DropShadow(2.0f, 2.0f, 1.5f, 1.5f,
                                    {0.02f, 0.08f, 0.12f, 0.75f});
  return filter;
}

void DrawSolidGeometryMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                            const tgfx::Color& color) {
  const auto size = bounds.width();
  auto base = Fill({color.red * 0.32f, color.green * 0.32f, color.blue * 0.32f, 0.30f});
  canvas->drawRoundRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.07f, bounds.top + size * 0.10f,
                           size * 0.78f, size * 0.62f),
      size * 0.18f, size * 0.12f, base);

  auto nonAA = Fill({color.red * 0.64f, color.green * 0.64f, color.blue * 0.64f, 0.86f});
  nonAA.setAntiAlias(false);
  canvas->drawRoundRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.10f, bounds.top + size * 0.12f,
                           size * 0.38f, size * 0.27f),
      size * 0.06f, size * 0.03f, nonAA);

  const std::array<tgfx::Point, 4> radii = {
      tgfx::Point{size * 0.24f, size * 0.08f},
      tgfx::Point{size * 0.03f, size * 0.03f},
      tgfx::Point{size * 0.17f, size * 0.23f},
      tgfx::Point{size * 0.05f, size * 0.18f},
  };
  auto complex = Fill({0.92f, 0.98f, 1.0f, 0.72f});
  canvas->drawRRect(
      tgfx::RRect::MakeRectRadii(
          tgfx::Rect::MakeXYWH(bounds.left + size * 0.20f,
                               bounds.top + size * (0.48f + 0.03f * std::sin(phase)),
                               size * 0.68f, size * 0.36f),
          radii),
      complex);

  auto complexNonAA = Fill({color.red * 0.42f, color.green * 0.42f,
                            color.blue * 0.42f, 0.82f});
  complexNonAA.setAntiAlias(false);
  const std::array<tgfx::Point, 4> nonAARadii = {
      tgfx::Point{size * 0.04f, size * 0.04f},
      tgfx::Point{size * 0.18f, size * 0.06f},
      tgfx::Point{size * 0.02f, size * 0.02f},
      tgfx::Point{size * 0.12f, size * 0.20f},
  };
  canvas->drawRRect(
      tgfx::RRect::MakeRectRadii(
          tgfx::Rect::MakeXYWH(bounds.left + size * 0.54f,
                               bounds.top + size * 0.10f,
                               size * 0.36f, size * 0.28f),
          nonAARadii),
      complexNonAA);
}

void DrawStrokeMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                     const tgfx::Color& color) {
  const auto size = bounds.width();
  auto roundRectStroke = Stroke({color.red, color.green, color.blue, 0.76f},
                                std::max(1.5f, size * 0.045f));
  roundRectStroke.setLineJoin(tgfx::LineJoin::Round);
  canvas->drawRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.12f, bounds.top + size * 0.12f,
                           size * 0.48f, size * 0.32f),
      roundRectStroke);

  auto ovalStroke = Stroke({0.92f, 0.89f, 0.76f, 0.56f},
                           std::max(1.0f, size * 0.025f));
  canvas->drawOval(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.48f, bounds.top + size * 0.14f,
                           size * 0.38f, size * 0.38f),
      ovalStroke);

  auto hairline = Stroke({0.92f, 0.89f, 0.76f, 0.44f}, 0.0f);
  hairline.setLineCap(tgfx::LineCap::Round);
  canvas->drawLine(bounds.left + size * 0.12f,
                   bounds.top + size * (0.62f + 0.05f * std::sin(phase)),
                   bounds.right - size * 0.10f,
                   bounds.top + size * 0.55f, hairline);

  tgfx::Path curve;
  curve.moveTo(bounds.left + size * 0.12f, bounds.top + size * 0.84f);
  curve.quadTo(bounds.left + size * 0.50f,
               bounds.top + size * (0.66f + 0.08f * std::cos(phase)),
               bounds.right - size * 0.12f, bounds.top + size * 0.82f);
  canvas->drawPath(curve, hairline);
}

void DrawGradientMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                       const tgfx::Color& color) {
  const auto size = bounds.width();
  const auto gap = size * 0.04f;
  const auto half = (size - gap) * 0.5f;
  const auto left = bounds.left + size * 0.05f;
  const auto top = bounds.top + size * 0.10f;
  const std::vector<tgfx::Color> colors = {
      color, {0.98f, 0.78f, 0.20f, 0.96f}, {0.18f, 0.82f, 0.64f, 0.92f}};

  auto linear = Fill(tgfx::Color::White());
  linear.setShader(tgfx::Shader::MakeLinearGradient(
      {left, top}, {left + half, top + half}, colors));
  canvas->drawRoundRect(tgfx::Rect::MakeXYWH(left, top, half, half),
                        size * 0.05f, size * 0.03f, linear);

  auto radial = Fill(tgfx::Color::White());
  radial.setShader(tgfx::Shader::MakeRadialGradient(
      {left + half + gap + half * 0.5f,
       top + half * (0.48f + 0.04f * std::sin(phase))},
      half * 0.50f, colors));
  canvas->drawOval(
      tgfx::Rect::MakeXYWH(left + half + gap, top, half, half), radial);

  auto conic = Fill(tgfx::Color::White());
  conic.setShader(tgfx::Shader::MakeConicGradient(
      {left + half * 0.5f, top + half + gap + half * 0.5f},
      phase * 12.0f, phase * 12.0f + 300.0f, colors));
  canvas->drawRoundRect(
      tgfx::Rect::MakeXYWH(left, top + half + gap, half, half),
      size * 0.02f, size * 0.10f, conic);

  auto diamond = Fill(tgfx::Color::White());
  diamond.setShader(tgfx::Shader::MakeDiamondGradient(
      {left + half + gap + half * 0.5f, top + half + gap + half * 0.5f},
      half * 0.54f, colors));
  canvas->drawRect(
      tgfx::Rect::MakeXYWH(left + half + gap, top + half + gap, half, half), diamond);
}

void DrawImageSamplingMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                            const std::shared_ptr<tgfx::Image>& image) {
  if (image == nullptr) {
    return;
  }
  const auto size = bounds.width();
  const auto imageSize = size * 0.50f;
  const auto src = ImageSource(image);
  const auto srcHalf = tgfx::Rect::MakeWH(src.width() * 0.55f, src.height() * 0.55f);
  auto paint = Fill(tgfx::Color::White());
  paint.setAlpha(0.96f);
  canvas->drawImageRect(
      image, srcHalf,
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.06f, bounds.top + size * 0.18f,
                           imageSize, imageSize),
      tgfx::SamplingOptions(tgfx::FilterMode::Nearest, tgfx::MipmapMode::None), &paint);
  canvas->save();
  canvas->rotate(8.0f * std::sin(phase), bounds.centerX(), bounds.centerY());
  canvas->drawImageRect(
      image,
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.44f, bounds.top + size * 0.18f,
                           imageSize, imageSize),
      tgfx::SamplingOptions(tgfx::FilterMode::Linear, tgfx::MipmapMode::Linear), &paint);
  canvas->restore();
}

void DrawTiledTextureMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                           const std::shared_ptr<tgfx::Image>& image) {
  if (image == nullptr) {
    return;
  }
  auto paint = Fill(tgfx::Color::White());
  paint.setShader(tgfx::Shader::MakeImageShader(
      image, tgfx::TileMode::Repeat, tgfx::TileMode::Mirror,
      tgfx::SamplingOptions(tgfx::FilterMode::Linear, tgfx::MipmapMode::None)));
  const auto inset = bounds.width() * 0.07f;
  canvas->save();
  canvas->rotate(4.0f * std::sin(phase), bounds.centerX(), bounds.centerY());
  canvas->clipRRect(
      tgfx::RRect::MakeRectXY(
          tgfx::Rect::MakeLTRB(bounds.left + inset, bounds.top + inset,
                               bounds.right - inset, bounds.bottom - inset),
          bounds.width() * 0.14f, bounds.width() * 0.08f),
      true);
  canvas->drawRect(bounds, paint);
  canvas->restore();
}

void DrawTextMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                   const tgfx::Font& font, const tgfx::Color& color, size_t index) {
  const auto size = bounds.width();
  const auto hasTypeface = font.getTypeface() != nullptr;
  if (!hasTypeface) {
    canvas->drawRoundRect(bounds, size * 0.10f, size * 0.08f,
                          Fill({color.red, color.green, color.blue, 0.86f}));
    return;
  }
  auto solid = Fill({0.95f, 0.93f, 0.84f, 0.82f});
  canvas->drawSimpleText(index % 2 == 0 ? "Aa" : "0x",
                         bounds.left + size * 0.10f,
                         bounds.top + size * 0.46f, font, solid);

  auto gradientPaint = Fill(color);
  gradientPaint.setShader(tgfx::Shader::MakeLinearGradient(
      {bounds.left, bounds.top},
      {bounds.right, bounds.bottom + size * 0.04f * std::sin(phase)},
      {{0.16f, 0.86f, 0.96f, 1.0f}, {0.98f, 0.72f, 0.18f, 1.0f}}));
  canvas->save();
  canvas->clipRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.08f,
                           bounds.top + size * 0.52f,
                           size * 0.78f, size * 0.30f),
      true);
  canvas->drawSimpleText("TGFX", bounds.left + size * 0.06f,
                         bounds.top + size * 0.82f, font, gradientPaint);
  canvas->restore();
}

void DrawColorFilterMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                          const std::shared_ptr<tgfx::Image>& image,
                          const tgfx::Color& color) {
  if (image != nullptr) {
    auto filtered = Fill(tgfx::Color::White());
    filtered.setColorFilter(GetShaderMixColorFilter());
    filtered.setAlpha(0.96f);
    canvas->drawImageRect(
        image,
        tgfx::Rect::MakeXYWH(bounds.left + bounds.width() * 0.08f,
                             bounds.top + bounds.width() * 0.10f,
                             bounds.width() * 0.56f, bounds.width() * 0.56f),
        tgfx::SamplingOptions(tgfx::FilterMode::Linear, tgfx::MipmapMode::None),
        &filtered);
    auto lumaOnly = Fill(tgfx::Color::White());
    lumaOnly.setColorFilter(tgfx::ColorFilter::Luma());
    canvas->drawImageRect(
        image,
        tgfx::Rect::MakeXYWH(bounds.left + bounds.width() * 0.48f,
                             bounds.top + bounds.width() * 0.08f,
                             bounds.width() * 0.26f, bounds.width() * 0.26f),
        tgfx::SamplingOptions(tgfx::FilterMode::Linear, tgfx::MipmapMode::None),
        &lumaOnly);
  }
  auto threshold = Fill({color.red, color.green, color.blue, 0.88f});
  threshold.setColorFilter(tgfx::ColorFilter::Blend(
      {0.95f, 0.98f, 0.22f, 0.92f}, tgfx::BlendMode::Screen));
  canvas->save();
  canvas->rotate(7.0f * std::sin(phase), bounds.centerX(), bounds.centerY());
  canvas->drawRoundRect(
      tgfx::Rect::MakeXYWH(bounds.left + bounds.width() * 0.48f,
                           bounds.top + bounds.width() * 0.48f,
                           bounds.width() * 0.40f, bounds.width() * 0.27f),
      bounds.width() * 0.06f, bounds.width() * 0.06f, threshold);
  canvas->restore();
}

void DrawPathAndMaskMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                          size_t index, const tgfx::Color& color) {
  const auto size = bounds.width();
  auto suit = Fill({color.red, color.green, color.blue, 0.72f});
  canvas->save();
  canvas->rotate(9.0f * std::sin(phase), bounds.centerX(), bounds.centerY());
  canvas->drawPath(
      SuitPath(index,
               tgfx::Rect::MakeXYWH(bounds.left + size * 0.08f,
                                    bounds.top + size * 0.10f,
                                    size * 0.34f, size * 0.34f)),
      suit);
  canvas->restore();

  tgfx::Path rings;
  rings.addOval(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.40f,
                           bounds.top + size * 0.10f,
                           size * 0.48f, size * 0.48f));
  rings.addOval(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.51f,
                           bounds.top + size * 0.21f,
                           size * 0.26f, size * 0.26f));
  rings.setFillType(tgfx::PathFillType::EvenOdd);
  canvas->drawPath(rings, Fill({0.92f, 0.89f, 0.76f, 0.58f}));

  tgfx::Path inverse;
  inverse.addRRect(tgfx::RRect::MakeRectXY(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.10f,
                           bounds.top + size * 0.58f,
                           size * 0.78f, size * 0.28f),
      size * 0.08f, size * 0.08f));
  inverse.toggleInverseFillType();
  canvas->save();
  canvas->clipRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.10f,
                           bounds.top + size * 0.58f,
                           size * 0.78f, size * 0.28f),
      true);
  canvas->drawPath(
      inverse,
      Fill({color.green * 0.45f, color.blue * 0.68f, 0.96f, 0.62f}));
  canvas->restore();
}

void DrawBlendAndClipMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                           const tgfx::Color& color, bool includeOffscreenDst) {
  const auto size = bounds.width();
  canvas->drawRoundRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.08f,
                           bounds.top + size * 0.12f,
                           size * 0.80f, size * 0.72f),
      size * 0.12f, size * 0.08f,
      Fill({color.red * 0.26f, color.green * 0.26f, color.blue * 0.30f, 0.64f}));

  canvas->save();
  canvas->clipRRect(tgfx::RRect::MakeRectXY(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.12f,
                           bounds.top + size * 0.16f,
                           size * 0.72f, size * 0.60f),
      size * 0.10f, size * 0.07f), true);
  const std::array<tgfx::BlendMode, 3> modes = {
      tgfx::BlendMode::Screen, tgfx::BlendMode::Multiply, tgfx::BlendMode::PlusLighter};
  for (size_t modeIndex = 0; modeIndex < modes.size(); ++modeIndex) {
    auto paint = Fill(Hsl(static_cast<float>(modeIndex) * 110.0f + phase * 18.0f,
                          0.82f, 0.58f, 0.72f));
    paint.setBlendMode(modes[modeIndex]);
    canvas->drawOval(
        tgfx::Rect::MakeXYWH(
            bounds.left + size * (0.14f + static_cast<float>(modeIndex) * 0.17f),
            bounds.top + size * (0.30f + 0.05f * std::sin(
                                                 phase + static_cast<float>(modeIndex))),
                             size * 0.42f, size * 0.34f),
        paint);
  }
  canvas->restore();

  if (includeOffscreenDst) {
    canvas->saveLayer(nullptr);
    canvas->drawRoundRect(
        tgfx::Rect::MakeXYWH(bounds.left + size * 0.46f,
                             bounds.top + size * 0.58f,
                             size * 0.34f, size * 0.24f),
        size * 0.05f, size * 0.05f,
        Fill({0.96f, 0.54f, 0.20f, 0.90f}));
    auto dstIn = Fill({0.92f, 0.98f, 0.98f, 0.90f});
    dstIn.setBlendMode(tgfx::BlendMode::DstIn);
    canvas->drawOval(
        tgfx::Rect::MakeXYWH(bounds.left + size * 0.54f,
                             bounds.top + size * 0.54f,
                             size * 0.34f, size * 0.34f),
        dstIn);
    canvas->restore();
  } else {
    canvas->drawRoundRect(
        tgfx::Rect::MakeXYWH(bounds.left + size * 0.48f,
                             bounds.top + size * 0.60f,
                             size * 0.30f, size * 0.20f),
        size * 0.04f, size * 0.04f,
        Fill({0.96f, 0.54f, 0.20f, 0.66f}));
  }
}

void DrawNoiseMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                    const tgfx::Color& color) {
  const auto size = bounds.width();
  auto fractal = Fill(tgfx::Color::White());
  fractal.setShader(tgfx::Shader::MakeFractalNoise(
      0.012f + 0.002f * std::sin(phase), 0.016f, 4, 7.0f));
  fractal.setColorFilter(tgfx::ColorFilter::Blend(
      {color.red, color.green, color.blue, 0.62f}, tgfx::BlendMode::Multiply));
  canvas->drawRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.08f,
                           bounds.top + size * 0.10f,
                           size * 0.82f, size * 0.38f),
      fractal);

  auto turbulence = Fill(tgfx::Color::White());
  turbulence.setShader(tgfx::Shader::MakeTurbulence(
      0.020f, 0.014f + 0.003f * std::cos(phase), 3, 19.0f));
  turbulence.setColorFilter(
      tgfx::ColorFilter::Compose(tgfx::ColorFilter::Luma(),
                                 tgfx::ColorFilter::AlphaThreshold(0.52f)));
  canvas->drawRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.18f,
                           bounds.top + size * 0.50f,
                           size * 0.64f, size * 0.38f),
      turbulence);
}

void DrawFilteredEffectsMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                              const tgfx::Color& color) {
  static_cast<void>(phase);
  const auto size = bounds.width();
  auto blurred = Fill({color.red, color.green, color.blue, 0.88f});
  blurred.setImageFilter(GetShaderMixBlurFilter());
  canvas->drawRoundRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.12f,
                           bounds.top + size * 0.16f,
                           size * 0.30f, size * 0.22f),
      size * 0.08f, size * 0.05f, blurred);

  auto shadow = Fill({0.96f, 0.98f, 1.0f, 0.96f});
  shadow.setImageFilter(GetShaderMixShadowFilter());
  canvas->drawPath(
      SuitPath(3,
               tgfx::Rect::MakeXYWH(bounds.left + size * 0.48f,
                                    bounds.top + size * 0.18f,
                                    size * 0.24f, size * 0.28f)),
      shadow);

  auto radialMask = tgfx::Shader::MakeRadialGradient(
      {bounds.left + size * 0.30f, bounds.top + size * 0.75f},
      size * 0.14f,
      {{1.0f, 1.0f, 1.0f, 0.95f}, {1.0f, 1.0f, 1.0f, 0.0f}});
  auto masked = Fill({0.22f, 0.84f, 0.92f, 0.92f});
  masked.setMaskFilter(tgfx::MaskFilter::MakeShader(std::move(radialMask)));
  canvas->drawRect(
      tgfx::Rect::MakeXYWH(bounds.left + size * 0.08f,
                           bounds.top + size * 0.56f,
                           size * 0.34f, size * 0.20f),
      masked);
}

void DrawMeshMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase) {
  const auto& mesh = GetShaderMixMesh();
  if (mesh == nullptr) {
    return;
  }
  const auto size = bounds.width() * 0.72f;
  canvas->save();
  canvas->translate(bounds.left + bounds.width() * 0.14f,
                    bounds.top + bounds.height() * 0.14f);
  canvas->scale(size, size * (0.94f + 0.04f * std::sin(phase)));
  canvas->drawMesh(mesh, Fill(tgfx::Color::White()));
  canvas->restore();
}

void DrawInstancingMotif(tgfx::Canvas* canvas, const tgfx::Rect& bounds, float phase,
                         size_t index, const tgfx::Color& color) {
  const auto size = bounds.width();
  auto ellipse = Fill({color.red * 0.75f, color.green * 0.75f, color.blue * 0.75f, 0.74f});
  for (size_t item = 0; item < 5; ++item) {
    const auto offset = static_cast<float>(item) / 5.0f;
    canvas->drawOval(
        tgfx::Rect::MakeXYWH(bounds.left + size * (0.08f + offset * 0.66f),
                             bounds.top + size * (0.12f + 0.05f * std::sin(phase + offset * PI)),
                             size * 0.18f, size * 0.18f),
        ellipse);
  }
  auto pathFill = Fill({color.red * 0.88f, color.green * 0.76f, color.blue * 0.48f, 0.66f});
  for (size_t item = 0; item < 4; ++item) {
    const auto offset = static_cast<float>(item) / 4.0f;
    canvas->drawPath(
        SuitPath(index + item,
                 tgfx::Rect::MakeXYWH(bounds.left + size * (0.10f + offset * 0.68f),
                                      bounds.top + size * 0.48f,
                                      size * 0.16f, size * 0.20f)),
        pathFill);
  }
}

void DrawShaderMixMotif(tgfx::Canvas* canvas, const ShaderMixTile& tile, size_t index,
                        const tgfx::Rect& bounds, float phase, const tgfx::Font& font,
                        const AppHost* host, bool includeExpensiveEffects) {
  const auto image = MotionMarkImage(host, tile.imageIndex);
  switch (tile.motif % kShaderMixMotifCount) {
    case 0:
      DrawSolidGeometryMotif(canvas, bounds, phase, tile.color);
      break;
    case 1:
      DrawStrokeMotif(canvas, bounds, phase, tile.color);
      break;
    case 2:
      DrawGradientMotif(canvas, bounds, phase, tile.color);
      break;
    case 3:
      DrawImageSamplingMotif(canvas, bounds, phase, image);
      break;
    case 4:
      DrawTiledTextureMotif(canvas, bounds, phase, image);
      break;
    case 5:
      DrawTextMotif(canvas, bounds, phase, font, tile.color, index);
      break;
    case 6:
      DrawColorFilterMotif(canvas, bounds, phase, image, tile.color);
      break;
    case 7:
      DrawPathAndMaskMotif(canvas, bounds, phase, index, tile.color);
      break;
    case 8:
      DrawBlendAndClipMotif(canvas, bounds, phase, tile.color, includeExpensiveEffects);
      break;
    case 9:
      DrawNoiseMotif(canvas, bounds, phase, tile.color);
      break;
    case 10:
      if (includeExpensiveEffects) {
        DrawFilteredEffectsMotif(canvas, bounds, phase, tile.color);
      } else {
        auto fallback = Fill({tile.color.red * 0.72f, tile.color.green * 0.72f,
                              tile.color.blue * 0.72f, 0.78f});
        canvas->drawRoundRect(bounds, bounds.width() * 0.12f,
                              bounds.width() * 0.08f, fallback);
      }
      break;
    case 11:
      DrawMeshMotif(canvas, bounds, phase);
      break;
    default:
      DrawInstancingMotif(canvas, bounds, phase, index, tile.color);
      break;
  }
}

}  // namespace

MultiplyBench::MultiplyBench() : Bench("MotionMark-Multiply") {
  ensureTileCapacity(count);
}

void MultiplyBench::ensureTileCapacity(size_t newCount) {
  if (tiles.size() >= newCount) {
    return;
  }
  if (tiles.capacity() < newCount) {
    tiles.reserve(std::max(newCount, tiles.capacity() * 2));
  }
  const auto previousSize = tiles.size();
  tiles.resize(newCount);
  constexpr size_t columns = 32;
  constexpr size_t rows = 16;
  constexpr size_t slots = columns * rows;
  for (size_t i = previousSize; i < newCount; ++i) {
    const auto slot = i % slots;
    const auto row = slot / columns;
    const auto column = slot % columns;
    auto& tile = tiles[i];
    const auto layer = i / slots;
    tile.normalizedX = (static_cast<float>(column) +
        (layer == 0 ? 0.5f : 0.1f + Unit(i, 4) * 0.8f)) / static_cast<float>(columns);
    tile.normalizedY = (static_cast<float>(row) +
        (layer == 0 ? 0.5f : 0.1f + Unit(i, 5) * 0.8f)) / static_cast<float>(rows);
    tile.rotation = Unit(i, 1) * 360.0f;
    tile.rotationSpeed = 0.18f + Unit(i, 2) * 0.85f;
    tile.radiusScale = 0.10f + Unit(i, 3) * 0.42f;
    const auto dx = tile.normalizedX - 0.5f;
    const auto dy = tile.normalizedY - 0.5f;
    tile.distance = std::sqrt(dx * dx + dy * dy);
    tile.shape = i % 6;
  }
}

void MultiplyBench::setCount(size_t newCount) {
  count = std::max<size_t>(1, newCount);
  ensureTileCapacity(count);
}

void MultiplyBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  const auto width = static_cast<float>(host->width());
  const auto height = static_cast<float>(host->height());
  canvas->drawRect(tgfx::Rect::MakeWH(width, height), Fill({0.055f, 0.065f, 0.075f, 1.0f}));
  const auto tileSize = std::max(7.0f, std::min(width / 32.0f, height / 16.0f) * 0.72f);
  const auto phase = static_cast<float>(animationFrame) * 0.012f;
  const auto driftX = width / 32.0f * 0.075f;
  const auto driftY = height / 16.0f * 0.075f;
  const auto margin = tileSize * 0.72f;
  for (size_t i = 0; i < count; ++i) {
    auto& tile = tiles[i];
    tile.rotation += tile.rotationSpeed * animationDelta;
    const auto wave = phase - tile.distance * 10.0f;
    const auto x = std::clamp(tile.normalizedX * width + std::sin(wave) * driftX,
                              margin, std::max(margin, width - margin));
    const auto y = std::clamp(tile.normalizedY * height + std::cos(wave) * driftY,
                              margin, std::max(margin, height - margin));
    const auto influence = std::max(0.01f, 1.0f - tile.distance * 1.5f);
    auto paint = Fill(Hsl(165.0f + tile.normalizedX * 140.0f + tile.normalizedY * 95.0f +
                              std::sin(wave) * 32.0f,
                          0.76f, 0.43f + influence * 0.16f + std::sin(wave) * 0.07f,
                          std::min(0.82f, 0.18f + influence)));
    canvas->save();
    canvas->translate(x, y);
    canvas->rotate(tile.rotation);
    const auto size = tileSize;
    const auto radius = size * tile.radiusScale;
    const auto bounds = tgfx::Rect::MakeXYWH(-size * 0.5f, -size * 0.5f, size, size);
    if (tile.shape == 4) {
      canvas->drawOval(bounds, paint);
    } else {
      canvas->drawRoundRect(bounds, radius, radius, paint);
    }
    canvas->restore();
  }
}

CanvasArcsBench::CanvasArcsBench() : Bench("MotionMark-CanvasArcs") {
  ensureArcCapacity(count);
}

void CanvasArcsBench::ensureArcCapacity(size_t newCount) {
  if (arcs.size() >= newCount) {
    return;
  }
  if (arcs.capacity() < newCount) {
    arcs.reserve(std::max(newCount, arcs.capacity() * 2));
  }
  const auto previousSize = arcs.size();
  arcs.resize(newCount);
  static const tgfx::Color accentColors[] = {
      {0.90f, 0.29f, 0.35f, 1.0f},
      {0.05f, 0.58f, 0.62f, 1.0f},
      {0.18f, 0.40f, 0.77f, 1.0f},
  };
  for (size_t i = previousSize; i < newCount; ++i) {
    auto& arc = arcs[i];
    arc.normalizedX = i < 8 ? 0.16f + static_cast<float>(i % 4) * 0.225f
                            : 0.08f + Fraction(0.5f + static_cast<float>(i) * 0.75487767f) * 0.84f;
    arc.normalizedY = i < 8 ? 0.30f + static_cast<float>(i / 4) * 0.40f
                            : 0.08f + Fraction(0.5f + static_cast<float>(i) * 0.56984030f) * 0.84f;
    arc.radiusScale = 0.018f + std::pow(Unit(i, 11), 5.0f) * 0.075f;
    arc.startAngle = Unit(i, 12) * 360.0f;
    arc.baseSweep = 90.0f + Unit(i, 13) * 210.0f;
    arc.omega = (i % 2 == 0 ? 1.0f : -1.0f) * (0.45f + Unit(i, 14) * 1.2f);
    arc.counterclockwise = Unit(i, 15) > 0.5f;
    arc.width = 1.0f + std::pow(Unit(i, 16), 5.0f) * 30.0f;
    arc.stroked = i % 4 != 0;
    const auto paletteIndex = (i / 3) % 4;
    const auto paletteValue = 0.16f + static_cast<float>(paletteIndex) * 0.22f;
    arc.color = i % 3 == 0 ? accentColors[(i / 3) % 3]
                           : tgfx::Color{paletteValue, paletteValue + 0.035f,
                                         paletteValue + 0.06f, 1.0f};
  }
}

void CanvasArcsBench::setCount(size_t newCount) {
  count = std::max<size_t>(1, newCount);
  ensureArcCapacity(count);
}

void CanvasArcsBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  const auto width = static_cast<float>(host->width());
  const auto height = static_cast<float>(host->height());
  canvas->drawRect(tgfx::Rect::MakeWH(width, height), Fill({0.965f, 0.973f, 0.98f, 1.0f}));
  const auto elapsed = static_cast<float>(animationFrame) / 60.0f;
  const auto minSize = std::min(width, height);
  for (size_t i = 0; i < count; ++i) {
    auto& arc = arcs[i];
    arc.startAngle = std::fmod(arc.startAngle + arc.omega * animationDelta, 360.0f);
    const auto radius = arc.radiusScale * minSize * 2.0f;
    const auto margin = radius + arc.width * 0.5f + 2.0f;
    const auto centerX = std::clamp(arc.normalizedX * width +
        std::sin(elapsed * 0.5f + static_cast<float>(i) * 0.8f) * minSize * 0.012f,
        margin, std::max(margin, width - margin));
    const auto centerY = std::clamp(arc.normalizedY * height +
        std::cos(elapsed * 0.4f + static_cast<float>(i) * 0.8f) * minSize * 0.012f,
        margin, std::max(margin, height - margin));
    const auto sweep = (arc.counterclockwise ? -1.0f : 1.0f) *
        (arc.baseSweep + std::sin(elapsed * 0.7f + static_cast<float>(i)) * 24.0f);
    tgfx::Path path;
    if (arc.stroked) {
      path.addArc(tgfx::Rect::MakeXYWH(centerX - radius, centerY - radius,
                                       radius * 2.0f, radius * 2.0f),
                  arc.startAngle, sweep);
      canvas->drawPath(path, Stroke(arc.color, arc.width));
    } else {
      path.addArc(tgfx::Rect::MakeXYWH(centerX - radius, centerY - radius,
                                       radius * 2.0f, radius * 2.0f),
                  arc.startAngle, sweep);
      path.lineTo(centerX, centerY);
      path.close();
      canvas->drawPath(path, Fill(arc.color));
    }
  }
}

LeavesBench::LeavesBench() : Bench("MotionMark-Leaves") {
}

void LeavesBench::resetParticle(MotionMarkLeaf& leaf, size_t index, float width, float height) const {
  leaf.size = 20.0f + std::pow(Unit(index, 21), 4.0f) * 30.0f;
  leaf.x = width * (0.04f + Fraction(0.5f + static_cast<float>(index) * 0.75487767f) * 0.92f);
  leaf.y = Unit(index, 23) * height;
  leaf.angle = Unit(index, 24) * 360.0f;
  leaf.angularSpeed = (Unit(index, 25) - 0.5f) * 65.0f;
  leaf.velocityX = -0.7f - Unit(index, 26) * 1.8f;
  leaf.velocityY = leaf.size * 0.1f + (Unit(index, 27) - 0.5f);
  leaf.imageIndex = index % ImageNames.size();
}

void LeavesBench::ensureParticleCapacity(size_t newCount, float width, float height) {
  if (width != canvasWidth || height != canvasHeight) {
    const auto oldWidth = std::max(1.0f, canvasWidth);
    const auto oldHeight = std::max(1.0f, canvasHeight);
    for (auto& particle : particles) {
      particle.x = particle.x / oldWidth * width;
      particle.y = particle.y / oldHeight * height;
    }
    canvasWidth = width;
    canvasHeight = height;
  }
  if (particles.size() >= newCount) {
    return;
  }
  if (particles.capacity() < newCount) {
    particles.reserve(std::max(newCount, particles.capacity() * 2));
  }
  const auto previousSize = particles.size();
  particles.resize(newCount);
  for (size_t i = previousSize; i < newCount; ++i) {
    resetParticle(particles[i], i, width, height);
  }
}

void LeavesBench::setCount(size_t newCount) {
  count = std::max<size_t>(1, newCount);
}

void LeavesBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  const auto width = static_cast<float>(host->width());
  const auto height = static_cast<float>(host->height());
  canvas->drawRect(tgfx::Rect::MakeWH(width, height), Fill({0.10f, 0.13f, 0.14f, 1.0f}));
  if (canvasWidth == 0.0f || canvasHeight == 0.0f) {
    canvasWidth = width;
    canvasHeight = height;
  }
  ensureParticleCapacity(count, width, height);
  const auto elapsed = static_cast<float>(animationFrame) / 60.0f;
  for (size_t i = 0; i < count; ++i) {
    auto& leaf = particles[i];
    const auto sway = std::sin(elapsed * 0.7f + static_cast<float>(i) * 0.62f);
    leaf.x += (leaf.velocityX + 1.4f * sway) * animationDelta * 0.45f;
    leaf.y += leaf.velocityY * animationDelta * 0.55f;
    leaf.angle += leaf.angularSpeed * animationDelta * 0.016f;
    const auto opacity = 0.64f + 0.30f * std::sin(elapsed * 0.55f + static_cast<float>(i) * 0.83f);
    const auto margin = std::min(leaf.size * 0.72f, std::min(width, height) * 0.45f);
    ReflectParticle(leaf.x, leaf.velocityX, margin, width - margin);
    ReflectParticle(leaf.y, leaf.velocityY, margin, height - margin);
    auto image = MotionMarkImage(host, leaf.imageIndex);
    auto paint = Fill(tgfx::Color::White());
    paint.setAlpha(opacity);
    canvas->save();
    canvas->translate(leaf.x, leaf.y);
    canvas->rotate(leaf.angle);
    const auto destination = tgfx::Rect::MakeXYWH(-leaf.size * 0.5f, -leaf.size * 0.5f,
                                                   leaf.size, leaf.size);
    if (image != nullptr) {
      canvas->drawImageRect(image, ImageSource(image), destination, {}, &paint);
    } else {
      canvas->drawCircle(0.0f, 0.0f, leaf.size * 0.35f, paint);
    }
    canvas->restore();
  }
}

CanvasLinesBench::CanvasLinesBench() : Bench("MotionMark-CanvasLines") {
  ensureLineCapacity(count);
}

void CanvasLinesBench::ensureLineCapacity(size_t newCount) {
  if (lines.size() >= newCount) {
    return;
  }
  if (lines.capacity() < newCount) {
    lines.reserve(std::max(newCount, lines.capacity() * 2));
  }
  const auto previousSize = lines.size();
  lines.resize(newCount);
  static const tgfx::Color colors[] = {
      {0.92f, 0.33f, 0.40f, 1.0f},
      {0.04f, 0.65f, 0.57f, 1.0f},
      {0.28f, 0.45f, 0.86f, 1.0f},
      {0.95f, 0.61f, 0.20f, 1.0f},
  };
  for (size_t i = previousSize; i < newCount; ++i) {
    auto& line = lines[i];
    line.circle = i % 4;
    line.theta = Fraction(static_cast<float>(i / 4) * 0.61803399f +
                          static_cast<float>(line.circle) * 0.125f) * PI * 2.0f;
    line.cosTheta = std::cos(line.theta);
    line.sinTheta = std::sin(line.theta);
    line.width = 3.0f + std::pow(Unit(i, 42), 12.0f) * 20.0f;
    line.omega = 0.2f + Unit(i, 43) * 3.0f;
    line.length = 20.0f + std::pow(Unit(i, 44), 8.0f) * 40.0f;
    line.direction = Unit(i, 45) > 0.5f ? -1.0f : 1.0f;
    line.color = colors[line.circle];
  }
}

void CanvasLinesBench::setCount(size_t newCount) {
  count = std::max<size_t>(1, newCount);
  ensureLineCapacity(count);
}

void CanvasLinesBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  const auto width = static_cast<float>(host->width());
  const auto height = static_cast<float>(host->height());
  const auto density = host->density();
  const auto bounds = tgfx::Rect::MakeWH(width, height);
  canvas->drawRect(bounds, Fill({0.96f, 0.97f, 0.98f, 1.0f}));
  const auto portrait = width < height * 1.1f;
  const float centerX[] = {width * (portrait ? 0.27f : 0.155f),
                           width * (portrait ? 0.73f : 0.385f),
                           width * (portrait ? 0.27f : 0.615f),
                           width * (portrait ? 0.73f : 0.845f)};
  const float centerY[] = {height * (portrait ? 0.30f : 0.66f),
                           height * (portrait ? 0.30f : 0.34f),
                           height * (portrait ? 0.70f : 0.66f),
                           height * (portrait ? 0.70f : 0.34f)};
  const auto radius = std::max(2.0f, portrait
      ? std::min(width * 0.145f, height * 0.12f)
      : std::min(width * 0.09f, height * 0.20f));
  const auto detailScale = std::min(density, std::min(width, height) / 500.0f);
  static const tgfx::Color strokeColors[] = {
      {0.92f, 0.33f, 0.40f, 1.0f},
      {0.04f, 0.65f, 0.57f, 1.0f},
      {0.28f, 0.45f, 0.86f, 1.0f},
      {0.95f, 0.61f, 0.20f, 1.0f},
  };
  static const tgfx::Color fillColors[] = {
      {0.29f, 0.11f, 0.15f, 1.0f},
      {0.035f, 0.24f, 0.21f, 1.0f},
      {0.09f, 0.15f, 0.32f, 1.0f},
      {0.30f, 0.21f, 0.085f, 1.0f},
  };
  const auto elapsedMs = static_cast<float>(animationFrame) * (1000.0f / 60.0f);
  const auto angle = elapsedMs / 3000.0f * PI * 2.0f;
  const auto dx = width * 0.4f * std::cos(angle);
  const auto dy = width * 0.4f * std::sin(angle);
  const auto gradientStep = 0.5f + 0.5f *
      std::sin(elapsedMs / 5000.0f * PI * 2.0f);
  const auto colorStopStep = Lerp(-0.1f, 0.1f, gradientStep);
  const auto brightness = Lerp(32.0f, 64.0f, gradientStep) / 255.0f;
  const tgfx::Color gradientStart = {
      brightness, brightness, brightness * 2.0f, 0.4f};
  const tgfx::Color gradientEnd = {
      brightness * 2.0f, brightness * 2.0f, brightness, 0.4f};
  const auto gradient = tgfx::Shader::MakeLinearGradient(
      {width * 0.5f + dx, height * 0.5f + dy},
      {width * 0.5f - dx, height * 0.5f - dy},
      {gradientStart, gradientStart, gradientEnd, gradientEnd},
      {0.0f, 0.2f + colorStopStep, 0.8f - colorStopStep, 1.0f});
  for (size_t i = 0; i < 4; ++i) {
    const auto oval = tgfx::Rect::MakeXYWH(centerX[i] - radius, centerY[i] - radius,
                                            radius * 2.0f, radius * 2.0f);
    canvas->drawOval(oval, Stroke(strokeColors[i], 15.0f * detailScale));
    canvas->drawOval(oval, Fill(fillColors[i]));
    tgfx::Paint paint = Fill(tgfx::Color::White());
    paint.setShader(gradient);
    canvas->drawOval(oval, paint);
  }
  for (size_t i = 0; i < count; ++i) {
    const auto& line = lines[i];
    const auto rotation = elapsedMs * 0.00018f * (line.circle % 2 == 0 ? 1.0f : -1.0f);
    const auto cosRotation = std::cos(rotation);
    const auto sinRotation = std::sin(rotation);
    const auto cosTheta = line.cosTheta * cosRotation - line.sinTheta * sinRotation;
    const auto sinTheta = line.sinTheta * cosRotation + line.cosTheta * sinRotation;
    const auto pulse = 0.5f + 0.5f * std::sin(elapsedMs * 0.0015f * line.omega + line.theta);
    const auto length = (line.length * 0.55f + (18.0f + line.length * 0.45f) * pulse) * detailScale;
    const auto startX = centerX[line.circle] + radius * cosTheta;
    const auto startY = centerY[line.circle] + radius * sinTheta;
    const auto endX = startX + line.direction * length * cosTheta;
    const auto endY = startY + line.direction * length * sinTheta;
    auto paint = Stroke(line.color, line.width * detailScale);
    paint.setLineCap(tgfx::LineCap::Square);
    canvas->drawLine(startX, startY, endX, endY, paint);
  }
}

ImagesBench::ImagesBench() : Bench("MotionMark-Images") {
  ensureTileCapacity(count);
}

void ImagesBench::ensureTileCapacity(size_t newCount) {
  if (tiles.size() >= newCount) {
    return;
  }
  if (tiles.capacity() < newCount) {
    tiles.reserve(std::max(newCount, tiles.capacity() * 2));
  }
  const auto previousSize = tiles.size();
  tiles.resize(newCount);
  for (size_t i = previousSize; i < newCount; ++i) {
    auto& tile = tiles[i];
    const auto slot = (i * 23) % 64;
    const auto layer = i / 64;
    tile.normalizedX = (static_cast<float>(slot % 8) + 0.5f +
        (layer == 0 ? 0.0f : (Unit(i, 50) - 0.5f) * 0.65f)) / 8.0f;
    tile.normalizedY = (static_cast<float>(slot / 8) + 0.5f +
        (layer == 0 ? 0.0f : (Unit(i, 56) - 0.5f) * 0.65f)) / 8.0f;
    tile.sizeScale = 0.72f + Unit(i, 51) * 0.75f;
    tile.angle = (Unit(i, 52) - 0.5f) * 24.0f;
    tile.angularSpeed = 8.0f + Unit(i, 53) * 12.0f;
    tile.orbitX = 0.006f + Unit(i, 54) * 0.015f;
    tile.orbitY = 0.008f + Unit(i, 55) * 0.018f;
    tile.imageIndex = i % ImageNames.size();
  }
}

void ImagesBench::setCount(size_t newCount) {
  count = std::max<size_t>(1, newCount);
  ensureTileCapacity(count);
}

void ImagesBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  const auto width = static_cast<float>(host->width());
  const auto height = static_cast<float>(host->height());
  canvas->drawRect(tgfx::Rect::MakeWH(width, height), Fill({0.07f, 0.085f, 0.105f, 1.0f}));
  const auto baseSize = std::clamp(std::min(width, height) / 15.0f, 24.0f, 76.0f);
  const auto phase = static_cast<float>(animationFrame) * 0.018f;
  for (size_t i = 0; i < count; ++i) {
    const auto& tile = tiles[i];
    const auto size = baseSize * tile.sizeScale;
    const auto margin = std::min(size * 0.72f, std::min(width, height) * 0.25f);
    const auto x = margin + tile.normalizedX * (width - margin * 2.0f) +
        std::sin(phase + static_cast<float>(i) * 0.31f) * tile.orbitX * width;
    const auto y = margin + tile.normalizedY * (height - margin * 2.0f) +
        std::cos(phase * 0.85f + static_cast<float>(i) * 0.27f) * tile.orbitY * height;
    auto image = MotionMarkImage(host, tile.imageIndex);
    auto paint = Fill(tgfx::Color::White());
    paint.setAlpha(0.72f + 0.28f * (0.5f + 0.5f * std::sin(
        phase + static_cast<float>(i) * 0.21f)));
    canvas->save();
    canvas->translate(std::clamp(x, margin, width - margin),
                      std::clamp(y, margin, height - margin));
    canvas->rotate(tile.angle + std::sin(phase * 0.65f + static_cast<float>(i)) * tile.angularSpeed);
    const auto destination =
        tgfx::Rect::MakeXYWH(-size * 0.5f, -size * 0.5f, size, size);
    if (image != nullptr) {
      canvas->drawImageRect(image, ImageSource(image), destination, {}, &paint);
    } else {
      canvas->drawRoundRect(destination, size * 0.12f, size * 0.12f,
                            Fill(Hsl(static_cast<float>(i) * 27.0f, 0.72f, 0.58f, 0.8f)));
    }
    canvas->restore();
  }
}

DesignBench::DesignBench() : Bench("MotionMark-Design") {
  labels = {
      u8"\u03c3\u03c7\u03ad\u03b4\u03b9\u03bf", u8"\u8bbe\u8ba1",
      u8"suunnittelu", u8"design",
      u8"\u0434\u0438\u0437\u0430\u0439\u043d", u8"\u062a\u0635\u0645\u064a\u0645",
      u8"\ub514\uc790\uc778", u8"conception",
      u8"\u30c7\u30b6\u30a4\u30f3", u8"konstruktion",
      u8"\u05e2\u05d9\u05e6\u05d5\u05d1", u8"dise\u00f1o"};
}

void DesignBench::setCount(size_t newCount) {
  count = std::max<size_t>(1, newCount);
}

void DesignBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  auto typeface = host->getTypeface("default");
  const auto width = static_cast<float>(host->width());
  const auto height = static_cast<float>(host->height());
  canvas->drawRect(tgfx::Rect::MakeWH(width, height), Fill({0.955f, 0.967f, 0.97f, 1.0f}));
  if (typeface == nullptr) {
    return;
  }
  const size_t columns = width < height * 1.1f ? 2 : 3;
  const auto rows = (labels.size() + columns - 1) / columns;
  const auto cellWidth = width * 0.90f / static_cast<float>(columns);
  const auto cellHeight = height * 0.84f / static_cast<float>(rows);
  auto fontSize = std::clamp(std::min(width, height) * 0.055f, 10.0f, 52.0f);
  fontSize = std::min(fontSize, cellWidth * 0.11f);
  const tgfx::Font font(typeface, fontSize);
  if (labelWidths.empty() || !(measuredFont == font)) {
    measuredFont = font;
    labelWidths.clear();
    for (const auto& label : labels) {
      const auto blob = tgfx::TextBlob::MakeFrom(label, font);
      labelWidths.push_back(blob == nullptr ? 0.0f : blob->getBounds().width());
    }
  }
  const auto rotation = static_cast<float>(animationFrame) * 0.0045f;
  const auto maxOffset = std::min(width, height) * 0.035f;
  const auto offsetX = std::cos(rotation) * maxOffset;
  const auto offsetY = std::sin(rotation) * maxOffset;
  const auto shimmer = static_cast<float>(animationFrame) * 0.014f;
  const auto layerAlpha = std::clamp(0.12f + 2.2f / static_cast<float>(count), 0.12f, 0.9f);
  for (size_t layer = 0; layer < count; ++layer) {
    const auto layerProgress = (static_cast<float>(layer) + 1.0f) / static_cast<float>(count);
    canvas->save();
    canvas->translate(width * 0.5f + offsetX * (layerProgress - 0.5f),
                      height * 0.5f + offsetY * (layerProgress - 0.5f));
    canvas->rotate((layerProgress - 0.5f) * 3.0f * std::sin(rotation * 0.7f));
    canvas->translate(-width * 0.5f, -height * 0.5f);
    for (size_t i = 0; i < labels.size(); ++i) {
      const auto column = i % columns;
      const auto row = i / columns;
      const auto baseX = width * 0.05f + (static_cast<float>(column) + 0.5f) * cellWidth;
      const auto baseY = height * 0.08f + (static_cast<float>(row) + 0.5f) * cellHeight;
      const auto progress = (static_cast<float>(layer) +
                             static_cast<float>(i) / static_cast<float>(labels.size())) /
                            std::max(1.0f, static_cast<float>(count));
      const auto colorProgress = std::clamp(
          progress + std::sin(shimmer + progress * 7.0f) * 0.12f, 0.0f, 1.0f);
      auto color = Hsl(165.0f + static_cast<float>(i % 4) * 44.0f + colorProgress * 36.0f,
                       0.62f, 0.34f + colorProgress * 0.10f, layerAlpha);
      auto paint = Fill(color);
      const auto textWidth = labelWidths[i];
      const auto x = baseX - textWidth * 0.5f +
                     std::sin(shimmer * 0.7f + static_cast<float>(i)) * fontSize * 0.08f;
      const auto y = baseY + fontSize * 0.3f +
          std::cos(shimmer * 0.55f + static_cast<float>(i)) * fontSize * 0.14f;
      canvas->drawSimpleText(labels[i], x, y, font, paint);
    }
    canvas->restore();
  }
}

SuitsBench::SuitsBench() : Bench("MotionMark-Suits") {
}

void SuitsBench::resetParticle(MotionMarkSuit& suit, size_t index, float width,
                               float height) const {
  suit.size = 30.0f + std::pow(Unit(index, 61), 4.0f) * 40.0f;
  suit.x = suit.size * 0.5f + Unit(index, 66) * std::max(1.0f, width - suit.size);
  suit.y = suit.size * 0.5f + Unit(index, 67) * std::max(1.0f, height - suit.size);
  const auto angle = Unit(index, 62) * PI * 2.0f +
                     static_cast<float>(animationFrame) * 0.6f;
  const auto velocity = 0.5f + Unit(index, 63) * 2.0f;
  suit.velocityX = std::sin(angle) * velocity * 3.0f;
  suit.velocityY = std::cos(angle) * velocity * 3.0f;
  suit.rotation = Unit(index, 64) * 360.0f;
  suit.angularSpeed = (Unit(index, 65) - 0.5f) * 85.0f;
  suit.shape = index % 4;
  const float hues[] = {350.0f, 169.0f, 220.0f, 38.0f};
  suit.color = Hsl(hues[suit.shape] + Unit(index, 68) * 16.0f, 0.72f, 0.55f);
}

void SuitsBench::ensureParticleCapacity(size_t newCount, float width, float height) {
  if (width != canvasWidth || height != canvasHeight) {
    const auto oldWidth = std::max(1.0f, canvasWidth);
    const auto oldHeight = std::max(1.0f, canvasHeight);
    for (auto& particle : particles) {
      particle.x = particle.x / oldWidth * width;
      particle.y = particle.y / oldHeight * height;
    }
    canvasWidth = width;
    canvasHeight = height;
  }
  if (particles.size() >= newCount) {
    return;
  }
  if (particles.capacity() < newCount) {
    particles.reserve(std::max(newCount, particles.capacity() * 2));
  }
  const auto previousSize = particles.size();
  particles.resize(newCount);
  for (size_t i = previousSize; i < newCount; ++i) {
    resetParticle(particles[i], i, width, height);
  }
}

void SuitsBench::setCount(size_t newCount) {
  count = std::max<size_t>(1, newCount);
}

void SuitsBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  const auto width = static_cast<float>(host->width());
  const auto height = static_cast<float>(host->height());
  canvas->drawRect(tgfx::Rect::MakeWH(width, height), Fill({0.965f, 0.975f, 0.985f, 1.0f}));
  if (canvasWidth == 0.0f || canvasHeight == 0.0f) {
    canvasWidth = width;
    canvasHeight = height;
  }
  ensureParticleCapacity(count, width, height);
  for (size_t i = 0; i < count; ++i) {
    auto& suit = particles[i];
    const auto phase = static_cast<float>(animationFrame) * 0.009f + static_cast<float>(i) * 0.41f;
    suit.x += suit.velocityX * animationDelta * 0.32f;
    suit.y += suit.velocityY * animationDelta * 0.32f;
    suit.rotation += suit.angularSpeed * animationDelta * 0.016f;
    const auto margin = std::min(suit.size * 0.72f, std::min(width, height) * 0.45f);
    ReflectParticle(suit.x, suit.velocityX, margin, width - margin);
    ReflectParticle(suit.y, suit.velocityY, margin, height - margin);
    canvas->save();
    canvas->translate(suit.x, suit.y);
    canvas->rotate(suit.rotation + 5.0f * std::sin(phase));
    const auto bounds = tgfx::Rect::MakeXYWH(-suit.size * 0.5f, -suit.size * 0.5f,
                                              suit.size, suit.size);
    auto path = SuitPath(suit.shape, bounds);
    auto start = suit.color;
    auto end = tgfx::Color{std::min(1.0f, start.red + 0.20f),
                            std::min(1.0f, start.green + 0.20f),
                            std::min(1.0f, start.blue + 0.20f), 1.0f};
    auto paint = Fill(start);
    if (i % 3 == 0) {
      paint.setShader(tgfx::Shader::MakeLinearGradient(
          {bounds.left, bounds.top}, {bounds.right, bounds.bottom}, {start, end}, {}));
    }
    canvas->drawPath(path, paint);
    canvas->restore();
  }
}

ShaderMixBench::ShaderMixBench() : Bench("ColdStart-ShaderMix") {
  ensureTileCapacity(count);
}

void ShaderMixBench::ensureTileCapacity(size_t newCount) {
  if (tiles.size() >= newCount) {
    return;
  }
  if (tiles.capacity() < newCount) {
    tiles.reserve(std::max(newCount, tiles.capacity() * 2));
  }
  const auto previousSize = tiles.size();
  tiles.resize(newCount);
  for (size_t i = previousSize; i < newCount; ++i) {
    auto& tile = tiles[i];
    tile.phase = Unit(i, 73) * PI * 2.0f;
    tile.speed = 0.35f + Unit(i, 74) * 0.8f;
    tile.imageIndex = i % ImageNames.size();
    tile.motif = i % kShaderMixMotifCount;
    tile.normalizedX = 0.065f + Fraction(0.5f + static_cast<float>(i) * 0.75487767f) * 0.87f;
    tile.normalizedY = 0.075f + Fraction(0.5f + static_cast<float>(i) * 0.56984030f) * 0.85f;
    tile.rotation = (Unit(i, 76) - 0.5f) * 14.0f;
    tile.sizeScale = ShaderMixScales[tile.motif] * (0.90f + Unit(i, 77) * 0.20f);
    tile.layer = ShaderMixLayer(tile.motif);
    tile.color =
        ShaderMixPaletteColor(tile.motif * 2 + i / kShaderMixMotifCount);
  }
}

void ShaderMixBench::setCount(size_t newCount) {
  count = std::max<size_t>(1, newCount);
  ensureTileCapacity(count);
}

void ShaderMixBench::onDraw(tgfx::Canvas* canvas, const AppHost* host) {
  const auto width = static_cast<float>(host->width());
  const auto height = static_cast<float>(host->height());
  ensureTileCapacity(count);
  const auto elapsed = static_cast<float>(animationFrame) / 60.0f;
  const auto minSize = std::min(width, height);
  const auto densityScale =
      std::clamp(std::sqrt(64.0f / static_cast<float>(std::max<size_t>(count, 1))),
                 0.65f, 1.0f);
  DrawShaderMixBackdrop(canvas, width, height, elapsed);
  auto typeface = host->getTypeface("default");
  tgfx::Font font;
  tgfx::Font wordmarkFont;
  if (typeface != nullptr) {
    font = tgfx::Font(typeface, std::clamp(minSize * 0.020f, 7.0f, 22.0f));
    wordmarkFont = tgfx::Font(typeface, std::clamp(minSize * 0.10f, 28.0f, 120.0f));
    canvas->drawSimpleText("TGFX", width * 0.045f, height * 0.135f, wordmarkFont,
                           Fill({0.78f, 0.86f, 0.90f, 0.08f}));
  }

  for (size_t layer = 0; layer < 4; ++layer) {
    for (size_t i = 0; i < count; ++i) {
      const auto& tile = tiles[i];
      if (tile.layer != layer) {
        continue;
      }
      const auto phase = elapsed * tile.speed + tile.phase;
      const auto size =
          minSize * tile.sizeScale * densityScale * (0.96f + 0.04f * std::sin(phase * 1.17f));
      const auto drift = minSize * 0.018f;
      const auto margin = size * 0.72f;
      const auto centerX = std::clamp(
          tile.normalizedX * width + std::sin(phase * 0.72f) * drift,
          margin, std::max(margin, width - margin));
      const auto centerY = std::clamp(
          tile.normalizedY * height + std::cos(phase * 0.63f) * drift,
          margin, std::max(margin, height - margin));
      const auto bounds = tgfx::Rect::MakeXYWH(-size * 0.5f, -size * 0.5f, size, size);
      canvas->save();
      canvas->translate(centerX, centerY);
      canvas->rotate(tile.rotation + std::sin(phase * 0.34f) * 4.0f);
      DrawShaderMixMotif(canvas, tile, i, bounds, phase, font, host, frame == 1);
      canvas->restore();
    }
  }
}

}  // namespace benchmark
