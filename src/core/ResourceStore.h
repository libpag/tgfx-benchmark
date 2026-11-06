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

#include <map>
#include <memory>
#include <string>
#include "tgfx/core/Image.h"
#include "tgfx/core/Typeface.h"

namespace benchmark {

/**
 * Loads scene assets from the benchmark resource directory. Assets are copied into this repository
 * instead of being read from the tgfx checkout, because two tgfx versions may ship different
 * assets and the results would no longer be comparable.
 */
class ResourceStore {
 public:
  explicit ResourceStore(std::string rootDir);

  std::string path(const std::string& relativePath) const;

  std::shared_ptr<tgfx::Image> image(const std::string& relativePath);

  std::shared_ptr<tgfx::Typeface> typeface(const std::string& relativePath);

 private:
  std::string _rootDir;
  std::map<std::string, std::shared_ptr<tgfx::Image>> _images = {};
  std::map<std::string, std::shared_ptr<tgfx::Typeface>> _typefaces = {};
};

}  // namespace benchmark
