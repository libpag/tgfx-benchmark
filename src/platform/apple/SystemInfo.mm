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
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include "core/report/SystemInfo.h"

namespace benchmark {
namespace {

std::string SysctlString(const char* name) {
  size_t length = 0;
  if (sysctlbyname(name, nullptr, &length, nullptr, 0) != 0 || length == 0) {
    return {};
  }
  std::string result(length, '\0');
  if (sysctlbyname(name, result.data(), &length, nullptr, 0) != 0) {
    return {};
  }
  while (!result.empty() && result.back() == '\0') {
    result.pop_back();
  }
  return result;
}

}  // namespace

std::string OperatingSystemName() {
  @autoreleasepool {
    auto info = [[NSProcessInfo alloc] init];
    auto version = [[info operatingSystemVersionString] UTF8String];
    [info release];
    if (version != nullptr) {
      return version;
    }
  }
  return "macOS";
}

std::string CpuName() {
  auto brand = SysctlString("machdep.cpu.brand_string");
  if (brand.empty()) {
    brand = SysctlString("hw.model");
  }
  return brand;
}

std::string ThermalStateName() {
  @autoreleasepool {
    auto info = [[NSProcessInfo alloc] init];
    auto state = [info thermalState];
    [info release];
    switch (state) {
      case NSProcessInfoThermalStateNominal:
        return "nominal";
      case NSProcessInfoThermalStateFair:
        return "fair";
      case NSProcessInfoThermalStateSerious:
        return "serious";
      case NSProcessInfoThermalStateCritical:
        return "critical";
      default:
        return "unknown";
    }
  }
}

uint64_t ProcessFootprintBytes() {
  task_vm_info_data_t info = {};
  mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
  auto result = task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info),
                          &count);
  if (result != KERN_SUCCESS) {
    return 0;
  }
  return static_cast<uint64_t>(info.phys_footprint);
}

}  // namespace benchmark
