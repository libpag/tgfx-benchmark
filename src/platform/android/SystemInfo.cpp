/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/system_properties.h>
#include <unistd.h>
#include "core/report/SystemInfo.h"

namespace benchmark {
namespace {

std::string Property(const char* name) {
  char value[PROP_VALUE_MAX] = {};
  auto length = __system_property_get(name, value);
  if (length <= 0) {
    return {};
  }
  return std::string(value, static_cast<size_t>(length));
}

}  // namespace

std::string OperatingSystemName() {
  auto release = Property("ro.build.version.release");
  auto sdk = Property("ro.build.version.sdk");
  if (release.empty()) {
    return "Android";
  }
  return "Android " + release + " (API " + sdk + ")";
}

std::string CpuName() {
  auto hardware = Property("ro.hardware");
  auto model = Property("ro.product.model");
  if (hardware.empty()) {
    return model;
  }
  return model + " / " + hardware;
}

std::string ThermalStateName() {
  // Android reports thermal status through a system service that is not reachable from native
  // code without a JNI handle, so the value is reported as unknown.
  return "unknown";
}

uint64_t ProcessFootprintBytes() {
  std::ifstream status("/proc/self/status");
  std::string line;
  while (std::getline(status, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      auto value = std::strtoull(line.c_str() + 6, nullptr, 10);
      return static_cast<uint64_t>(value) * 1024U;
    }
  }
  return 0;
}

}  // namespace benchmark
