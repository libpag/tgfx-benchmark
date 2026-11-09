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

#include "core/SceneRegistry.h"
#include <algorithm>
#include "tgfx/platform/Print.h"

namespace benchmark {

const char* TierName(Tier tier) {
  switch (tier) {
    case Tier::S:
      return "S";
    case Tier::M:
      return "M";
    case Tier::L:
      return "L";
    case Tier::XL:
      return "XL";
  }
  return "M";
}

bool ParseTier(const std::string& text, Tier* tier) {
  if (tier == nullptr) {
    return false;
  }
  if (text == "S" || text == "s") {
    *tier = Tier::S;
    return true;
  }
  if (text == "M" || text == "m") {
    *tier = Tier::M;
    return true;
  }
  if (text == "L" || text == "l") {
    *tier = Tier::L;
    return true;
  }
  if (text == "XL" || text == "xl") {
    *tier = Tier::XL;
    return true;
  }
  return false;
}

std::map<std::string, SceneRegistry::Factory>& SceneRegistry::Factories() {
  static auto factories = std::map<std::string, Factory>();
  return factories;
}

bool SceneRegistry::Register(Factory factory) {
  if (!factory) {
    return false;
  }
  auto scene = factory();
  if (scene == nullptr) {
    return false;
  }
  auto id = scene->info().id;
  if (id.empty()) {
    // Silent drops made a scene with a broken info() invisible, so say it out loud.
    tgfx::PrintError("SceneRegistry: a scene registered an empty id and was ignored");
    return false;
  }
  Factories()[id] = std::move(factory);
  return true;
}

const std::vector<std::string>& SceneRegistry::Names() {
  static std::vector<std::string> names = [] {
    std::vector<std::string> result;
    for (const auto& item : Factories()) {
      result.push_back(item.first);
    }
    std::sort(result.begin(), result.end());
    return result;
  }();
  return names;
}

std::unique_ptr<Scene> SceneRegistry::Create(const std::string& id) {
  auto it = Factories().find(id);
  if (it == Factories().end()) {
    return nullptr;
  }
  return it->second();
}

bool SceneRegistry::Matches(const std::string& id, const std::vector<std::string>& patterns) {
  if (patterns.empty()) {
    return true;
  }
  for (const auto& pattern : patterns) {
    if (pattern == id) {
      return true;
    }
    if (!pattern.empty() && pattern.back() == '*') {
      auto prefix = pattern.substr(0, pattern.size() - 1);
      if (id.compare(0, prefix.size(), prefix) == 0) {
        return true;
      }
    }
  }
  return false;
}

std::vector<std::string> SceneRegistry::Resolve(const std::vector<std::string>& patterns) {
  std::vector<std::string> result;
  for (const auto& id : Names()) {
    if (Matches(id, patterns)) {
      result.push_back(id);
    }
  }
  return result;
}

}  // namespace benchmark
