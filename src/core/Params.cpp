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

#include "core/Params.h"
#include <cmath>
#include <cstdio>
#include "tgfx/platform/Print.h"

namespace benchmark {
namespace {

std::string FormatDouble(double value) {
  char buffer[64] = {};
  if (std::abs(value - std::round(value)) < 1e-9) {
    std::snprintf(buffer, sizeof(buffer), "%.1f", value);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.6g", value);
  }
  return buffer;
}

std::string EscapeString(const std::string& text) {
  std::string result;
  result.reserve(text.size() + 2);
  for (auto c : text) {
    switch (c) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += c;
        break;
    }
  }
  return result;
}

}  // namespace

std::string ToJSON(const ParamValue& value) {
  if (auto intValue = std::get_if<int64_t>(&value)) {
    return std::to_string(*intValue);
  }
  if (auto doubleValue = std::get_if<double>(&value)) {
    return FormatDouble(*doubleValue);
  }
  if (auto boolValue = std::get_if<bool>(&value)) {
    return *boolValue ? "true" : "false";
  }
  return "\"" + EscapeString(std::get<std::string>(value)) + "\"";
}

void ParamSet::setInt(const std::string& name, int64_t value) {
  _values[name] = value;
}

void ParamSet::setDouble(const std::string& name, double value) {
  _values[name] = value;
}

void ParamSet::setBool(const std::string& name, bool value) {
  _values[name] = value;
}

void ParamSet::setString(const std::string& name, std::string value) {
  _values[name] = std::move(value);
}

bool ParamSet::has(const std::string& name) const {
  return _values.count(name) > 0;
}

int64_t ParamSet::getInt(const std::string& name) const {
  auto result = _values.find(name);
  if (result == _values.end()) {
    tgfx::PrintError("ParamSet::getInt() parameter \"%s\" not found!", name.c_str());
    return 0;
  }
  if (auto value = std::get_if<int64_t>(&result->second)) {
    return *value;
  }
  if (auto value = std::get_if<double>(&result->second)) {
    return static_cast<int64_t>(*value);
  }
  tgfx::PrintError("ParamSet::getInt() parameter \"%s\" is not numeric!", name.c_str());
  return 0;
}

double ParamSet::getDouble(const std::string& name) const {
  auto result = _values.find(name);
  if (result == _values.end()) {
    tgfx::PrintError("ParamSet::getDouble() parameter \"%s\" not found!", name.c_str());
    return 0.0;
  }
  if (auto value = std::get_if<double>(&result->second)) {
    return *value;
  }
  if (auto value = std::get_if<int64_t>(&result->second)) {
    return static_cast<double>(*value);
  }
  tgfx::PrintError("ParamSet::getDouble() parameter \"%s\" is not numeric!", name.c_str());
  return 0.0;
}

bool ParamSet::getBool(const std::string& name) const {
  auto result = _values.find(name);
  if (result == _values.end()) {
    tgfx::PrintError("ParamSet::getBool() parameter \"%s\" not found!", name.c_str());
    return false;
  }
  if (auto value = std::get_if<bool>(&result->second)) {
    return *value;
  }
  tgfx::PrintError("ParamSet::getBool() parameter \"%s\" is not boolean!", name.c_str());
  return false;
}

std::string ParamSet::getString(const std::string& name) const {
  auto result = _values.find(name);
  if (result == _values.end()) {
    tgfx::PrintError("ParamSet::getString() parameter \"%s\" not found!", name.c_str());
    return {};
  }
  if (auto value = std::get_if<std::string>(&result->second)) {
    return *value;
  }
  tgfx::PrintError("ParamSet::getString() parameter \"%s\" is not a string!", name.c_str());
  return {};
}

std::string ParamSet::toCanonicalJSON() const {
  std::string result = "{";
  bool first = true;
  for (const auto& item : _values) {
    if (!first) {
      result += ",";
    }
    first = false;
    result += "\"" + EscapeString(item.first) + "\":" + ToJSON(item.second);
  }
  result += "}";
  return result;
}

std::string ParamSet::hash() const {
  const auto text = toCanonicalJSON();
  uint64_t hashValue = 0xcbf29ce484222325ULL;
  for (auto c : text) {
    hashValue ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
    hashValue *= 0x100000001b3ULL;
  }
  char buffer[17] = {};
  std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hashValue));
  return buffer;
}

}  // namespace benchmark
