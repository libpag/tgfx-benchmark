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

#include <cstdio>
#include <deque>
#include <string>
#include <vector>
#include "core/GpuTimer.h"
#include "tgfx/platform/Print.h"

// Only meaningful for the OpenGL backend; the other backends compile an empty factory instead.
#if defined(BENCH_BACKEND_GL)

#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>

namespace benchmark {
namespace {

constexpr int MaxPendingQueries = 8;

bool SupportsTimerQuery() {
  const auto* version = glGetString(GL_VERSION);
  if (version == nullptr) {
    return false;
  }
  auto major = 0;
  auto minor = 0;
  if (std::sscanf(reinterpret_cast<const char*>(version), "%d.%d", &major, &minor) != 2) {
    return false;
  }
  if (major > 3 || (major == 3 && minor >= 3)) {
    return true;
  }
  const auto* extensions = glGetString(GL_EXTENSIONS);
  if (extensions == nullptr) {
    return false;
  }
  return std::string(reinterpret_cast<const char*>(extensions)).find("GL_ARB_timer_query") !=
         std::string::npos;
}

class GLTimerQuery : public GpuTimer {
 public:
  GLTimerQuery() {
    for (auto& query : _queries) {
      glGenQueries(1, &query);
    }
  }

  ~GLTimerQuery() override {
    for (auto& query : _queries) {
      if (query != 0) {
        glDeleteQueries(1, &query);
      }
    }
  }

  const char* source() const override {
    return "gl_timer_query";
  }

  void Begin() override {
    if (_active) {
      return;
    }
    auto& query = _queries[_next];
    if (query == 0) {
      return;
    }
    glBeginQuery(GL_TIME_ELAPSED, query);
    _active = true;
  }

  void End() override {
    if (!_active) {
      return;
    }
    glEndQuery(GL_TIME_ELAPSED);
    _active = false;
    _pending.push_back(_queries[_next]);
    _next = (_next + 1) % MaxPendingQueries;
  }

  void Collect(std::vector<double>* samples, bool force) override {
    if (samples == nullptr) {
      return;
    }
    if (force) {
      glFinish();
    }
    auto count = static_cast<int>(_pending.size());
    for (auto i = 0; i < count; i++) {
      auto query = _pending.front();
      GLuint ready = 0;
      glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &ready);
      if (ready == 0 && !force) {
        break;
      }
      GLuint64 nanoseconds = 0;
      glGetQueryObjectui64v(query, GL_QUERY_RESULT, &nanoseconds);
      samples->push_back(static_cast<double>(nanoseconds) / 1000000.0);
      _pending.pop_front();
    }
  }

 private:
  GLuint _queries[MaxPendingQueries] = {};
  std::deque<GLuint> _pending = {};
  int _next = 0;
  bool _active = false;
};

}  // namespace

std::unique_ptr<GpuTimer> CreateGpuTimer() {
  if (!SupportsTimerQuery()) {
    return nullptr;
  }
  return std::make_unique<GLTimerQuery>();
}

}  // namespace benchmark

#else

namespace benchmark {

std::unique_ptr<GpuTimer> CreateGpuTimer() {
  return nullptr;
}

}  // namespace benchmark

#endif
