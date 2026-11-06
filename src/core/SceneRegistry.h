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

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "core/Scene.h"

namespace benchmark {

class SceneRegistry {
 public:
  using Factory = std::function<std::unique_ptr<Scene>()>;

  static bool Register(Factory factory);

  /// Returns all registered scene ids in lexicographic order.
  static const std::vector<std::string>& Names();

  static std::unique_ptr<Scene> Create(const std::string& id);

  /// Returns true when the id matches any of the glob patterns. An empty pattern list matches all.
  static bool Matches(const std::string& id, const std::vector<std::string>& patterns);

  /// Expands the patterns into a sorted list of scene ids.
  static std::vector<std::string> Resolve(const std::vector<std::string>& patterns);

 private:
  static std::map<std::string, Factory>& Factories();
};

}  // namespace benchmark

/// Registers a scene class. Place this at the end of the scene implementation file.
#define BENCH_SCENE(ClassName)                                                       \
  [[maybe_unused]] static const bool ClassName##SceneRegistered =                    \
      ::benchmark::SceneRegistry::Register([]() -> std::unique_ptr<::benchmark::Scene> { \
        return std::make_unique<ClassName>();                                        \
      })
