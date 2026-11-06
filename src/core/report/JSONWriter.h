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
#include <string>
#include <vector>

namespace benchmark {

std::string EscapeJSONString(const std::string& text);

/// A minimal streaming JSON writer. Kept dependency-free so the report format stays stable
/// regardless of which JSON library tgfx vendors.
class JSONWriter {
 public:
  JSONWriter() = default;

  void beginObject();
  void endObject();
  void beginArray();
  void endArray();

  void key(const std::string& name);

  void value(const std::string& text);
  void value(const char* text);
  void value(double number);
  void value(int64_t number);
  void value(uint64_t number);
  void value(int number);
  void value(bool flag);

  /// Appends an already serialized JSON fragment.
  void valueRaw(const std::string& raw);

  const std::string& str() const {
    return _out;
  }

 private:
  void prepareElement();

  std::string _out = {};
  std::vector<bool> _firstFlags = {};
  bool _afterKey = false;
};

}  // namespace benchmark
