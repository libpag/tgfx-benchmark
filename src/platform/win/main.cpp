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

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <iostream>
#include "TGFXWindow.h"
#if WINVER >= 0x0603  // Windows 8.1
#include <shellscalingapi.h>
#endif

extern "C" {
//Enable dedicated graphics for NVIDIA:
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
//Enable dedicated graphics for AMD:
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ForceHighPerformanceCore() {
  // Get the current process handle
  HANDLE hProcess = GetCurrentProcess();

  // Set the process priority class to high
  if (!SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS)) {
    std::cerr << "Failed to set process priority class." << std::endl;
    return;
  }

  // Get the system's processor information
  SYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer[256];
  DWORD returnLength = sizeof(buffer);
  if (!GetLogicalProcessorInformation(buffer, &returnLength)) {
    std::cerr << "Failed to get logical processor information." << std::endl;
    return;
  }

  // Iterate through the processor information to find high-performance cores
  DWORD_PTR highPerformanceMask = 0;
  int processorCount = returnLength / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
  for (int i = 0; i < processorCount; ++i) {
    if (buffer[i].Relationship == RelationProcessorCore) {
      // Assuming the first core is the high-performance core
      highPerformanceMask |= buffer[i].ProcessorMask;
      break;
    }
  }

  // Set the process affinity mask to use the high-performance core
  if (!SetProcessAffinityMask(hProcess, highPerformanceMask)) {
    std::cerr << "Failed to set process affinity mask." << std::endl;
    return;
  }

  std::cout << "Process is set to use high-performance core." << std::endl;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
#if WINVER >= 0x0603  // Windows 8.1
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#else
  SetProcessDPIAware();
#endif
  ForceHighPerformanceCore();

  benchmark::TGFXWindow tgfxWindow = {};
  tgfxWindow.open();

  MSG msg = {};
  while (GetMessage(&msg, nullptr, 0, 0) > 0) {
    if (msg.message == WM_QUIT) {
      return (int)msg.wParam;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}