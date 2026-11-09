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

#include "core/CommandLine.h"
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace benchmark {
namespace {

std::vector<std::string> SplitList(const std::string& text) {
  std::vector<std::string> result;
  std::string current;
  for (auto c : text) {
    if (c == ',') {
      if (!current.empty()) {
        result.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    result.push_back(current);
  }
  return result;
}

bool ParseResolution(const std::string& text, int* width, int* height) {
  auto position = text.find('x');
  if (position == std::string::npos) {
    return false;
  }
  auto widthText = text.substr(0, position);
  auto heightText = text.substr(position + 1);
  if (widthText.empty() || heightText.empty()) {
    return false;
  }
  *width = std::atoi(widthText.c_str());
  *height = std::atoi(heightText.c_str());
  return *width > 0 && *height > 0;
}

RunMode ParseMode(const std::string& text, bool* ok) {
  *ok = true;
  if (text == "measure") {
    return RunMode::Measure;
  }
  if (text == "capacity") {
    return RunMode::Capacity;
  }
  if (text == "first-frame" || text == "firstframe") {
    return RunMode::FirstFrame;
  }
  if (text == "interactive") {
    return RunMode::Interactive;
  }
  *ok = false;
  return RunMode::Interactive;
}

}  // namespace

const char* RunModeName(RunMode mode) {
  switch (mode) {
    case RunMode::Interactive:
      return "interactive";
    case RunMode::Measure:
      return "measure";
    case RunMode::Capacity:
      return "capacity";
    case RunMode::FirstFrame:
      return "first_frame";
  }
  return "measure";
}

bool ParseCommandLine(int argc, const char* argv[], CommandLineOptions* options,
                      std::string* error) {
  if (options == nullptr) {
    return false;
  }
  for (auto i = 1; i < argc; i++) {
    std::string argument = argv[i] == nullptr ? "" : argv[i];
    if (argument == "--help" || argument == "-h") {
      options->showHelp = true;
      continue;
    }
    if (argument == "--list") {
      options->listScenes = true;
      continue;
    }
    if (argument == "--dump-spec") {
      options->dumpSpec = true;
      continue;
    }
    if (argument == "--breakdown") {
      options->config.breakdownPass = true;
      continue;
    }
    if (argument == "--samples") {
      options->config.keepSamples = true;
      continue;
    }
    if (argument == "--no-breakdown") {
      options->config.breakdownPass = false;
      continue;
    }
    if (argument == "--no-samples") {
      options->config.keepSamples = false;
      continue;
    }
    if (argument == "--isolate") {
      options->config.isolate = true;
      continue;
    }
    if (argument.rfind("--", 0) != 0) {
      *error = "unexpected argument: " + argument;
      return false;
    }

    auto position = argument.find('=');
    if (position == std::string::npos) {
      *error = "missing value for: " + argument;
      return false;
    }
    auto name = argument.substr(2, position - 2);
    auto value = argument.substr(position + 1);

    if (name == "mode") {
      bool ok = false;
      options->config.mode = ParseMode(value, &ok);
      options->modeSpecified = true;
      if (!ok) {
        *error = "unknown mode: " + value;
        return false;
      }
    } else if (name == "suite") {
      options->config.suite = value;
    } else if (name == "filter") {
      options->config.filters = SplitList(value);
    } else if (name == "tier") {
      options->config.tiers.clear();
      for (const auto& item : SplitList(value)) {
        Tier tier = Tier::M;
        if (!ParseTier(item, &tier)) {
          *error = "unknown tier: " + item;
          return false;
        }
        options->config.tiers.push_back(tier);
      }
    } else if (name == "resolution") {
      if (!ParseResolution(value, &options->config.width, &options->config.height)) {
        *error = "invalid resolution: " + value;
        return false;
      }
    } else if (name == "density") {
      options->config.density = static_cast<float>(std::atof(value.c_str()));
    } else if (name == "warmup") {
      options->config.warmupFrames = std::atoi(value.c_str());
    } else if (name == "frames") {
      options->config.measureFrames = std::atoi(value.c_str());
    } else if (name == "repeat") {
      options->config.repeats = std::atoi(value.c_str());
    } else if (name == "budget") {
      options->config.budgetMs = std::atof(value.c_str());
    } else if (name == "out") {
      options->config.outPath = value;
    } else if (name == "checkpoint-dir") {
      options->config.checkpointDir = value;
    } else if (name == "resources") {
      options->config.resourcesDir = value;
    } else if (name == "single") {
      auto position = value.find(':');
      if (position == std::string::npos) {
        *error = "invalid --single value, expected <scene>:<tier>";
        return false;
      }
      options->config.singleCase = value.substr(0, position);
      options->config.singleTier = value.substr(position + 1);
      options->modeSpecified = true;
    } else {
      *error = "unknown option: --" + name;
      return false;
    }
  }

  if (options->config.tiers.empty()) {
    options->config.tiers.push_back(Tier::M);
  }
  if (options->config.repeats < 1) {
    options->config.repeats = 1;
  }
  if (options->config.measureFrames < 1) {
    options->config.measureFrames = 1;
  }
  return true;
}

void PrintUsage() {
  std::cout <<
      "TGFX Benchmark\n"
      "\n"
      "Without arguments the interactive windowed benchmark is started. With --mode the\n"
      "benchmark runs headless and writes a JSON result file.\n"
      "\n"
      "  --list                      list scenes and their parameters\n"
      "  --dump-spec                 write the scene specification as JSON\n"
      "  --mode=<mode>               measure | first-frame\n"
      "                              first-frame runs one case per child process and reports the\n"
      "                              cost of the very first frame, including pipeline creation.\n"
      "                              Compare it only against runs launched the same way.\n"
      "  --suite=<name>              label recorded in the result file\n"
      "  --filter=<glob,...>         scene ids, supports a trailing '*' (default: all)\n"
      "  --tier=<S,M,L,XL>           intensity tiers to run (default: M)\n"
      "  --resolution=<WxH>          offscreen size in pixels (default: 1920x1080)\n"
      "  --density=<float>           logical to physical pixel ratio (default: 2)\n"
      "  --warmup=<frames>           warmup frames per repeat (default: 60)\n"
      "  --frames=<frames>           measured frames per repeat (default: 300)\n"
      "  --repeat=<count>            process-level repeats (default: 5)\n"
      "  --budget=<ms>               frame budget for capacity mode (default: 16.67)\n"
      "  --out=<file>                result JSON path\n"
      "  --checkpoint-dir=<dir>      writes checkpoint thumbnails as raw RGBA\n"
      "  --resources=<dir>           overrides the compiled in resource directory\n"
      "  --no-breakdown              skips the second, CPU synchronized pass\n"
      "  --no-samples                omits raw per-frame samples from the result\n";
}

}  // namespace benchmark
