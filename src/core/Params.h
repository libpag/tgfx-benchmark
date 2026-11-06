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

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace benchmark {

using ParamValue = std::variant<int64_t, double, bool, std::string>;

/**
 * Describes one tunable parameter of a scene. Every scene declares exactly one parameter with
 * isScalingAxis set; the capacity mode varies that parameter and keeps the others fixed.
 */
struct ParamSpec {
  std::string name;
  ParamValue defaultValue;
  std::string description;
  bool isScalingAxis = false;
};

std::string ToJSON(const ParamValue& value);

/**
 * An ordered set of parameter values. Keys are always sorted, so toCanonicalJSON() produces a
 * stable representation that can be hashed and used as the primary key for cross-version
 * comparisons.
 */
class ParamSet {
 public:
  void setInt(const std::string& name, int64_t value);
  void setDouble(const std::string& name, double value);
  void setBool(const std::string& name, bool value);
  void setString(const std::string& name, std::string value);

  bool has(const std::string& name) const;
  int64_t getInt(const std::string& name) const;
  double getDouble(const std::string& name) const;
  bool getBool(const std::string& name) const;
  std::string getString(const std::string& name) const;

  /// Returns a compact JSON object with keys sorted alphabetically.
  std::string toCanonicalJSON() const;

  /// Returns a 16-character hexadecimal FNV-1a hash of toCanonicalJSON().
  std::string hash() const;

 private:
  std::map<std::string, ParamValue> _values = {};
};

}  // namespace benchmark
