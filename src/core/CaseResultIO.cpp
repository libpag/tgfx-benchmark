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
//  either express or implied. See the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "core/CaseResultIO.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "tgfx/platform/Print.h"

namespace benchmark {
namespace {

constexpr const char* kMagic = "bench-case-result-1";

std::string JoinDoubles(const std::vector<double>& values) {
  std::string result;
  for (size_t i = 0; i < values.size(); i++) {
    if (i > 0) {
      result += ' ';
    }
    result += std::to_string(values[i]);
  }
  return result;
}

std::string JoinStats(const SampleStats& stats) {
  std::ostringstream stream;
  stream.precision(17);
  stream << stats.count << ' ' << stats.min << ' ' << stats.median << ' ' << stats.mean << ' '
         << stats.p90 << ' ' << stats.p99 << ' ' << stats.max << ' ' << stats.stddev << ' '
         << stats.mad << ' ' << stats.cv;
  return stream.str();
}

std::vector<double> SplitDoubles(const std::string& text) {
  std::vector<double> values;
  std::istringstream stream(text);
  double value = 0.0;
  while (stream >> value) {
    values.push_back(value);
  }
  return values;
}

SampleStats ParseStats(const std::string& text) {
  SampleStats stats = {};
  std::istringstream stream(text);
  stream >> stats.count >> stats.min >> stats.median >> stats.mean >> stats.p90 >> stats.p99 >>
      stats.max >> stats.stddev >> stats.mad >> stats.cv;
  return stats;
}

}  // namespace

bool CaseResultIO::Write(const std::string& path, const CaseResult& result) {
  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    tgfx::PrintError("CaseResultIO: failed to open \"%s\"", path.c_str());
    return false;
  }
  stream.precision(17);
  stream << kMagic << '\n';
  stream << "scene " << result.scene << '\n';
  stream << "category " << result.category << '\n';
  stream << "tier " << result.tier << '\n';
  stream << "params " << result.paramsJSON << '\n';
  stream << "param_hash " << result.paramHash << '\n';
  stream << "status " << result.status << '\n';
  stream << "status_detail " << (result.statusDetail.empty() ? "-" : result.statusDetail) << '\n';
  stream << "setup_ms " << result.setupMs << '\n';
  stream << "throughput_per_repeat " << JoinDoubles(result.throughputPerRepeat) << '\n';
  stream << "throughput_blocks " << JoinDoubles(result.throughputBlocks) << '\n';
  stream << "throughput_median " << result.throughputMedian << '\n';
  stream << "frame_ms " << JoinStats(result.frameMs) << '\n';
  stream << "record_ms " << JoinStats(result.recordMs) << '\n';
  stream << "flush_ms " << JoinStats(result.flushMs) << '\n';
  stream << "submit_ms " << JoinStats(result.submitMs) << '\n';
  stream << "gpu_ms " << JoinStats(result.gpuMs) << '\n';
  stream << "first_frame_ms " << JoinStats(result.firstFrameMs) << '\n';
  stream << "gpu_source " << result.gpuSource << '\n';
  stream << "gpu_cache " << result.gpuCacheBytes << ' ' << result.gpuCachePeakBytes << '\n';
  stream << "footprint " << result.footprintBytes << ' ' << result.footprintPeakBytes << '\n';
  stream << "samples " << JoinDoubles(result.samples) << '\n';
  for (const auto& checkpoint : result.checkpoints) {
    stream << "checkpoint " << checkpoint.frame << ' ' << checkpoint.hash << ' '
           << (checkpoint.blank ? 1 : 0) << ' '
           << (checkpoint.fileName.empty() ? "-" : checkpoint.fileName) << '\n';
  }
  stream << "end\n";
  return stream.good();
}

bool CaseResultIO::Read(const std::string& path, CaseResult* result) {
  if (result == nullptr) {
    return false;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    tgfx::PrintError("CaseResultIO: failed to open \"%s\"", path.c_str());
    return false;
  }
  std::string line;
  if (!std::getline(stream, line) || line != kMagic) {
    tgfx::PrintError("CaseResultIO: \"%s\" is not a case result file", path.c_str());
    return false;
  }
  CaseResult parsed = {};
  while (std::getline(stream, line)) {
    auto space = line.find(' ');
    if (space == std::string::npos) {
      continue;
    }
    auto key = line.substr(0, space);
    auto value = line.substr(space + 1);
    if (key == "scene") {
      parsed.scene = value;
    } else if (key == "category") {
      parsed.category = value;
    } else if (key == "tier") {
      parsed.tier = value;
    } else if (key == "params") {
      parsed.paramsJSON = value;
    } else if (key == "param_hash") {
      parsed.paramHash = value;
    } else if (key == "status") {
      parsed.status = value;
    } else if (key == "status_detail") {
      parsed.statusDetail = value == "-" ? "" : value;
    } else if (key == "setup_ms") {
      parsed.setupMs = std::atof(value.c_str());
    } else if (key == "throughput_per_repeat") {
      parsed.throughputPerRepeat = SplitDoubles(value);
    } else if (key == "throughput_blocks") {
      parsed.throughputBlocks = SplitDoubles(value);
    } else if (key == "throughput_median") {
      parsed.throughputMedian = std::atof(value.c_str());
    } else if (key == "frame_ms") {
      parsed.frameMs = ParseStats(value);
    } else if (key == "record_ms") {
      parsed.recordMs = ParseStats(value);
    } else if (key == "flush_ms") {
      parsed.flushMs = ParseStats(value);
    } else if (key == "submit_ms") {
      parsed.submitMs = ParseStats(value);
    } else if (key == "first_frame_ms") {
      parsed.firstFrameMs = ParseStats(value);
    } else if (key == "gpu_ms") {
      parsed.gpuMs = ParseStats(value);
    } else if (key == "gpu_source") {
      parsed.gpuSource = value;
    } else if (key == "gpu_cache") {
      std::istringstream items(value);
      items >> parsed.gpuCacheBytes >> parsed.gpuCachePeakBytes;
    } else if (key == "footprint") {
      std::istringstream items(value);
      items >> parsed.footprintBytes >> parsed.footprintPeakBytes;
    } else if (key == "samples") {
      parsed.samples = SplitDoubles(value);
    } else if (key == "checkpoint") {
      std::istringstream items(value);
      CheckpointResult checkpoint = {};
      int blank = 0;
      items >> checkpoint.frame >> checkpoint.hash >> blank;
      std::string fileName;
      items >> fileName;
      checkpoint.blank = blank != 0;
      checkpoint.fileName = fileName == "-" ? "" : fileName;
      parsed.checkpoints.push_back(checkpoint);
    } else if (key == "end") {
      break;
    }
  }
  *result = std::move(parsed);
  return true;
}

}  // namespace benchmark
