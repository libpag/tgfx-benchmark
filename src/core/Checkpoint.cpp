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

#include "core/Checkpoint.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include "tgfx/core/ImageInfo.h"
#include "tgfx/platform/Print.h"

namespace benchmark {
namespace {

uint64_t HashBytes(const uint8_t* bytes, size_t count) {
  uint64_t hashValue = 0xcbf29ce484222325ULL;
  for (size_t i = 0; i < count; i++) {
    hashValue ^= static_cast<uint64_t>(bytes[i]);
    hashValue *= 0x100000001b3ULL;
  }
  return hashValue;
}

std::string ToHex(uint64_t value) {
  char buffer[17] = {};
  std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
  return buffer;
}

}  // namespace

CheckpointResult Checkpoint::Capture(tgfx::Surface* surface, const std::string& directory,
                                     const std::string& caseId, int64_t frame,
                                     const tgfx::Color& background) {
  CheckpointResult result = {};
  result.frame = frame;
  if (surface == nullptr) {
    result.hash = "no-surface";
    return result;
  }
  auto width = surface->width();
  auto height = surface->height();
  if (width <= 0 || height <= 0) {
    result.hash = "empty-surface";
    return result;
  }
  auto info = tgfx::ImageInfo::Make(width, height, tgfx::ColorType::RGBA_8888,
                                    tgfx::AlphaType::Premultiplied);
  std::vector<uint8_t> pixels(info.byteSize());
  if (!surface->readPixels(info, pixels.data())) {
    result.hash = "readback-failed";
    return result;
  }

  result.hash = ToHex(HashBytes(pixels.data(), pixels.size()));

  auto rowBytes = static_cast<size_t>(width) * 4;
  uint8_t reference[4] = {};
  for (size_t channel = 0; channel < 4; channel++) {
    auto value = background[static_cast<int>(channel)] * 255.0f + 0.5f;
    if (value < 0.0f) {
      value = 0.0f;
    } else if (value > 255.0f) {
      value = 255.0f;
    }
    reference[channel] = static_cast<uint8_t>(value);
  }
  size_t differentPixels = 0;
  for (auto y = 0; y < height; y++) {
    auto row = pixels.data() + static_cast<size_t>(y) * rowBytes;
    for (auto x = 0; x < width; x++) {
      auto pixel = row + static_cast<size_t>(x) * 4;
      for (size_t channel = 0; channel < 4; channel++) {
        auto delta = static_cast<int>(pixel[channel]) - static_cast<int>(reference[channel]);
        if (delta > 8 || delta < -8) {
          differentPixels++;
          break;
        }
      }
    }
  }
  auto totalPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
  result.blank = totalPixels > 0 && differentPixels * 1000 < totalPixels;

  if (directory.empty()) {
    return result;
  }

  std::error_code error = {};
  std::filesystem::create_directories(directory, error);
  auto fileName = caseId + "-" + std::to_string(frame) + ".rgba";
  auto filePath = std::filesystem::path(directory) / fileName;

  std::vector<uint8_t> thumbnail(static_cast<size_t>(ThumbnailWidth) *
                                 static_cast<size_t>(ThumbnailHeight) * 4);
  for (auto y = 0; y < ThumbnailHeight; y++) {
    auto sourceY = static_cast<int>((static_cast<int64_t>(y) * height) / ThumbnailHeight);
    if (sourceY >= height) {
      sourceY = height - 1;
    }
    for (auto x = 0; x < ThumbnailWidth; x++) {
      auto sourceX = static_cast<int>((static_cast<int64_t>(x) * width) / ThumbnailWidth);
      if (sourceX >= width) {
        sourceX = width - 1;
      }
      auto source = pixels.data() + static_cast<size_t>(sourceY) * rowBytes +
                    static_cast<size_t>(sourceX) * 4;
      auto target = thumbnail.data() +
                    (static_cast<size_t>(y) * static_cast<size_t>(ThumbnailWidth) +
                     static_cast<size_t>(x)) * 4;
      for (size_t channel = 0; channel < 4; channel++) {
        target[channel] = source[channel];
      }
    }
  }

  std::ofstream stream(filePath, std::ios::binary);
  if (!stream) {
    tgfx::PrintError("Checkpoint: failed to write \"%s\"", filePath.string().c_str());
    return result;
  }
  stream.write(reinterpret_cast<const char*>(thumbnail.data()),
               static_cast<std::streamsize>(thumbnail.size()));
  result.fileName = fileName;
  return result;
}

}  // namespace benchmark
