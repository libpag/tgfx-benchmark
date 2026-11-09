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

#import <UIKit/UIKit.h>
#include <cstdio>
#include <string>
#include "core/CommandLine.h"
#include "core/HeadlessApp.h"
#include "core/report/ReportWriter.h"

#import "AppDelegate.h"

#ifndef BENCH_RESOURCES_DIR
#define BENCH_RESOURCES_DIR ""
#endif

int main(int argc, char* argv[]) {
  // On a device argc/argv are empty, so the interactive window is the only path. Under the
  // simulator the binary can be launched with arguments to run the headless modes instead.
  benchmark::CommandLineOptions options = {};
  std::string error;
  if (!benchmark::ParseCommandLine(argc, const_cast<const char**>(argv), &options, &error)) {
    std::fprintf(stderr, "%s\n\n", error.c_str());
    benchmark::PrintUsage();
    return 2;
  }
  if (options.showHelp) {
    benchmark::PrintUsage();
    return 0;
  }
  if (options.listScenes) {
    benchmark::PrintSceneList();
    return 0;
  }
  if (options.dumpSpec) {
    auto spec = benchmark::ReportWriter::BuildSpec();
    if (options.config.outPath.empty()) {
      std::printf("%s\n", spec.c_str());
      return 0;
    }
    return benchmark::ReportWriter::Write(options.config.outPath, spec) ? 0 : 4;
  }
  if (options.modeSpecified && options.config.mode != benchmark::RunMode::Interactive) {
    return benchmark::RunHeadless(options, BENCH_RESOURCES_DIR);
  }
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}
