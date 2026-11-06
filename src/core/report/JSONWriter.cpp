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

#include "core/report/JSONWriter.h"
#include <cmath>
#include <cstdio>

namespace benchmark {

std::string EscapeJSONString(const std::string& text) {
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
        if (static_cast<unsigned char>(c) < 0x20) {
          char buffer[8] = {};
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
          result += buffer;
        } else {
          result += c;
        }
        break;
    }
  }
  return result;
}

void JSONWriter::prepareElement() {
  if (_firstFlags.empty()) {
    return;
  }
  if (_firstFlags.back()) {
    _firstFlags.back() = false;
  } else {
    _out += ',';
  }
}

void JSONWriter::beginObject() {
  if (!_afterKey) {
    prepareElement();
  }
  _afterKey = false;
  _out += '{';
  _firstFlags.push_back(true);
}

void JSONWriter::endObject() {
  _out += '}';
  if (!_firstFlags.empty()) {
    _firstFlags.pop_back();
  }
}

void JSONWriter::beginArray() {
  if (!_afterKey) {
    prepareElement();
  }
  _afterKey = false;
  _out += '[';
  _firstFlags.push_back(true);
}

void JSONWriter::endArray() {
  _out += ']';
  if (!_firstFlags.empty()) {
    _firstFlags.pop_back();
  }
}

void JSONWriter::key(const std::string& name) {
  prepareElement();
  _out += '"';
  _out += EscapeJSONString(name);
  _out += "\":";
  _afterKey = true;
}

void JSONWriter::value(const std::string& text) {
  if (!_afterKey) {
    prepareElement();
  }
  _afterKey = false;
  _out += '"';
  _out += EscapeJSONString(text);
  _out += '"';
}

void JSONWriter::value(const char* text) {
  value(std::string(text == nullptr ? "" : text));
}

void JSONWriter::value(double number) {
  if (!_afterKey) {
    prepareElement();
  }
  _afterKey = false;
  char buffer[64] = {};
  if (!std::isfinite(number)) {
    _out += "null";
    return;
  }
  std::snprintf(buffer, sizeof(buffer), "%.6g", number);
  _out += buffer;
}

void JSONWriter::value(int64_t number) {
  if (!_afterKey) {
    prepareElement();
  }
  _afterKey = false;
  _out += std::to_string(number);
}

void JSONWriter::value(uint64_t number) {
  if (!_afterKey) {
    prepareElement();
  }
  _afterKey = false;
  _out += std::to_string(number);
}

void JSONWriter::value(int number) {
  value(static_cast<int64_t>(number));
}

void JSONWriter::value(bool flag) {
  if (!_afterKey) {
    prepareElement();
  }
  _afterKey = false;
  _out += flag ? "true" : "false";
}

void JSONWriter::valueRaw(const std::string& raw) {
  if (!_afterKey) {
    prepareElement();
  }
  _afterKey = false;
  _out += raw;
}

}  // namespace benchmark
