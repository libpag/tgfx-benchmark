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

#include "core/ResourceStore.h"
#include "tgfx/platform/Print.h"

namespace benchmark {

ResourceStore::ResourceStore(std::string rootDir) : _rootDir(std::move(rootDir)) {
  while (!_rootDir.empty() && _rootDir.back() == '/') {
    _rootDir.pop_back();
  }
}

std::string ResourceStore::path(const std::string& relativePath) const {
  return _rootDir + "/" + relativePath;
}

std::shared_ptr<tgfx::Image> ResourceStore::image(const std::string& relativePath) {
  auto it = _images.find(relativePath);
  if (it != _images.end()) {
    return it->second;
  }
  auto image = tgfx::Image::MakeFromFile(path(relativePath));
  if (image == nullptr) {
    tgfx::PrintError("ResourceStore: failed to load image \"%s\"", relativePath.c_str());
    return nullptr;
  }
  _images[relativePath] = image;
  return image;
}

std::shared_ptr<tgfx::Data> ResourceStore::data(const std::string& relativePath) {
  auto it = _data.find(relativePath);
  if (it != _data.end()) {
    return it->second;
  }
  auto bytes = tgfx::Data::MakeFromFile(path(relativePath));
  if (bytes == nullptr) {
    tgfx::PrintError("ResourceStore: failed to read \"%s\"", relativePath.c_str());
    return nullptr;
  }
  _data[relativePath] = bytes;
  return bytes;
}

std::shared_ptr<tgfx::Typeface> ResourceStore::typeface(const std::string& relativePath) {
  auto it = _typefaces.find(relativePath);
  if (it != _typefaces.end()) {
    return it->second;
  }
  auto typeface = tgfx::Typeface::MakeFromPath(path(relativePath), 0);
  if (typeface == nullptr) {
    tgfx::PrintError("ResourceStore: failed to load typeface \"%s\"", relativePath.c_str());
    return nullptr;
  }
  _typefaces[relativePath] = typeface;
  return typeface;
}

}  // namespace benchmark
