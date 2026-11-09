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

#include <windows.h>
#include <psapi.h>
#include <string>
#include "core/report/SystemInfo.h"

namespace benchmark {
namespace {

std::string WideToUtf8(const std::wstring& text) {
  if (text.empty()) {
    return {};
  }
  auto length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                    nullptr, 0, nullptr, nullptr);
  if (length <= 0) {
    return {};
  }
  std::string result(static_cast<size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(),
                      length, nullptr, nullptr);
  return result;
}

std::string RegistryString(HKEY root, const wchar_t* path, const wchar_t* name) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(root, path, 0, KEY_READ, &key) != ERROR_SUCCESS) {
    return {};
  }
  wchar_t buffer[256] = {};
  DWORD size = sizeof(buffer);
  DWORD type = 0;
  auto status = RegQueryValueExW(key, name, nullptr, &type,
                                 reinterpret_cast<LPBYTE>(buffer), &size);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS || type != REG_SZ) {
    return {};
  }
  return WideToUtf8(buffer);
}

}  // namespace

std::string OperatingSystemName() {
  auto build = RegistryString(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                              L"CurrentBuildNumber");
  auto product = RegistryString(HKEY_LOCAL_MACHINE,
                                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                                L"ProductName");
  if (product.empty()) {
    return "Windows";
  }
  return product + " (build " + build + ")";
}

std::string CpuName() {
  auto cpu = RegistryString(HKEY_LOCAL_MACHINE,
                            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                            L"ProcessorNameString");
  return cpu;
}

std::string ThermalStateName() {
  // No managed thermal API is reachable without user mode telemetry; reported as unknown.
  return "unknown";
}

uint64_t ProcessFootprintBytes() {
  PROCESS_MEMORY_COUNTERS_EX counters = {};
  if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                           sizeof(counters)) == 0) {
    return 0;
  }
  return static_cast<uint64_t>(counters.PrivateUsage);
}

}  // namespace benchmark
