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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "core/report/ReportWriter.h"
#include <ctime>
#include <fstream>
#include "core/SceneRegistry.h"
#include "core/report/JSONWriter.h"
#include "tgfx/platform/Print.h"

namespace benchmark {
namespace {

void WriteStats(JSONWriter* writer, const std::string& name, const SampleStats& stats) {
  writer->key(name);
  writer->beginObject();
  writer->key("count");
  writer->value(stats.count);
  writer->key("min");
  writer->value(stats.min);
  writer->key("median");
  writer->value(stats.median);
  writer->key("mean");
  writer->value(stats.mean);
  writer->key("p90");
  writer->value(stats.p90);
  writer->key("p99");
  writer->value(stats.p99);
  writer->key("max");
  writer->value(stats.max);
  writer->key("stddev");
  writer->value(stats.stddev);
  writer->key("mad");
  writer->value(stats.mad);
  writer->key("cv");
  writer->value(stats.cv);
  writer->endObject();
}

}  // namespace

std::string ReportWriter::CurrentRunId() {
  auto now = std::time(nullptr);
  char buffer[32] = {};
  std::tm localTime = {};
#if defined(_WIN32)
  localtime_s(&localTime, &now);
#else
  localtime_r(&now, &localTime);
#endif
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &localTime);
  return buffer;
}

std::string ReportWriter::BuildResult(const EnvironmentInfo& environment,
                                      const RunConfig& config, const std::string& runId,
                                      const std::vector<CaseResult>& results) {
  JSONWriter writer;
  writer.beginObject();
  writer.key("schema_version");
  writer.value(1);
  writer.key("run_id");
  writer.value(runId);

  writer.key("env");
  writer.beginObject();
  writer.key("tgfx_commit");
  writer.value(environment.tgfxCommit);
  writer.key("benchmark_commit");
  writer.value(environment.benchmarkCommit);
  writer.key("backend");
  writer.value(environment.backend);
  writer.key("build_type");
  writer.value(environment.buildType);
  writer.key("os");
  writer.value(environment.os);
  writer.key("cpu");
  writer.value(environment.cpu);
  writer.key("gpu_renderer");
  writer.value(environment.gpuRenderer);
  writer.key("gpu_version");
  writer.value(environment.gpuVersion);
  writer.key("gpu_vendor");
  writer.value(environment.gpuVendor);
  writer.key("resolution");
  writer.beginArray();
  writer.value(environment.width);
  writer.value(environment.height);
  writer.endArray();
  writer.key("density");
  writer.value(static_cast<double>(environment.density));
  writer.key("thermal_start");
  writer.value(environment.thermalStart);
  writer.key("thermal_end");
  writer.value(environment.thermalEnd);
  writer.endObject();

  writer.key("config");
  writer.beginObject();
  writer.key("mode");
  writer.value(RunModeName(config.mode));
  writer.key("suite");
  writer.value(config.suite);
  writer.key("warmup");
  writer.value(config.warmupFrames);
  writer.key("measure");
  writer.value(config.measureFrames);
  writer.key("repeats");
  writer.value(config.repeats);
  writer.key("budget_ms");
  writer.value(config.budgetMs);
  writer.endObject();

  writer.key("results");
  writer.beginArray();
  for (const auto& result : results) {
    writer.beginObject();
    writer.key("scene");
    writer.value(result.scene);
    writer.key("category");
    writer.value(result.category);
    writer.key("tier");
    writer.value(result.tier);
    writer.key("params");
    writer.valueRaw(result.paramsJSON.empty() ? "{}" : result.paramsJSON);
    writer.key("param_hash");
    writer.value(result.paramHash);
    writer.key("status");
    writer.value(result.status);
    writer.key("status_detail");
    writer.value(result.statusDetail);
    writer.key("setup_ms");
    writer.value(result.setupMs);

    writer.key("throughput_ms");
    writer.beginObject();
    writer.key("per_repeat");
    writer.beginArray();
    for (auto value : result.throughputPerRepeat) {
      writer.value(value);
    }
    writer.endArray();
    writer.key("median");
    writer.value(result.throughputMedian);
    writer.key("blocks");
    writer.beginArray();
    for (auto value : result.throughputBlocks) {
      writer.value(value);
    }
    writer.endArray();
    writer.endObject();

    WriteStats(&writer, "frame_ms", result.frameMs);
    writer.key("breakdown_ms");
    writer.beginObject();
    WriteStats(&writer, "record", result.recordMs);
    WriteStats(&writer, "flush", result.flushMs);
    WriteStats(&writer, "submit", result.submitMs);
    writer.endObject();

    writer.key("gpu_ms");
    writer.beginObject();
    writer.key("median");
    writer.value(result.gpuMs.median);
    writer.key("p90");
    writer.value(result.gpuMs.p90);
    writer.key("source");
    writer.value(result.gpuSource);
    writer.endObject();

    WriteStats(&writer, "first_frame_ms", result.firstFrameMs);

    if (!result.capacity.axis.empty()) {
      writer.key("capacity");
      writer.beginObject();
      writer.key("axis");
      writer.value(result.capacity.axis);
      writer.key("budget_ms");
      writer.value(result.capacity.budgetMs);
      writer.key("metric");
      writer.value(result.capacity.metric);
      writer.key("max_value");
      writer.value(result.capacity.maxValue);
      writer.key("lower_bound");
      writer.value(result.capacity.lowerBound);
      writer.key("upper_bound");
      writer.value(result.capacity.upperBound);
      writer.key("probes");
      writer.value(result.capacity.probes);
      writer.key("saturated");
      writer.value(result.capacity.saturated);
      writer.endObject();
    }

    writer.key("memory");
    writer.beginObject();
    writer.key("gpu_cache_bytes");
    writer.value(result.gpuCacheBytes);
    writer.key("gpu_cache_peak_bytes");
    writer.value(result.gpuCachePeakBytes);
    writer.key("process_footprint_bytes");
    writer.value(result.footprintBytes);
    writer.key("process_peak_bytes");
    writer.value(result.footprintPeakBytes);
    writer.endObject();

    writer.key("checkpoints");
    writer.beginArray();
    for (const auto& checkpoint : result.checkpoints) {
      writer.beginObject();
      writer.key("frame");
      writer.value(checkpoint.frame);
      writer.key("hash");
      writer.value(checkpoint.hash);
      writer.key("blank");
      writer.value(checkpoint.blank);
      writer.key("file");
      writer.value(checkpoint.fileName);
      writer.endObject();
    }
    writer.endArray();

    if (config.keepSamples) {
      writer.key("samples");
      writer.beginObject();
      writer.key("frame_ms");
      writer.beginArray();
      for (auto value : result.samples) {
        writer.value(value);
      }
      writer.endArray();
      writer.endObject();
    }
    writer.endObject();
  }
  writer.endArray();
  writer.endObject();
  return writer.str();
}

std::string ReportWriter::BuildSpec() {
  JSONWriter writer;
  writer.beginObject();
  writer.key("schema_version");
  writer.value(1);
  writer.key("scenes");
  writer.beginArray();
  for (const auto& id : SceneRegistry::Names()) {
    auto scene = SceneRegistry::Create(id);
    if (scene == nullptr) {
      continue;
    }
    const auto& info = scene->info();
    writer.beginObject();
    writer.key("id");
    writer.value(info.id);
    writer.key("category");
    writer.value(info.category);
    writer.key("description");
    writer.value(info.description);
    writer.key("real_workload");
    writer.value(info.realWorkload);
    writer.key("modules");
    writer.value(static_cast<int64_t>(info.modules));
    writer.key("bottlenecks");
    writer.value(static_cast<int64_t>(info.bottlenecks));
    writer.key("params");
    writer.beginArray();
    for (const auto& param : info.params) {
      writer.beginObject();
      writer.key("name");
      writer.value(param.name);
      writer.key("default");
      writer.valueRaw(ToJSON(param.defaultValue));
      writer.key("description");
      writer.value(param.description);
      writer.key("scaling_axis");
      writer.value(param.isScalingAxis);
      writer.endObject();
    }
    writer.endArray();
    writer.key("tiers");
    writer.beginObject();
    for (auto tier : {Tier::S, Tier::M, Tier::L, Tier::XL}) {
      writer.key(TierName(tier));
      writer.valueRaw(scene->paramsForTier(tier).toCanonicalJSON());
    }
    writer.endObject();
    writer.endObject();
  }
  writer.endArray();
  writer.endObject();
  return writer.str();
}

bool ReportWriter::Write(const std::string& path, const std::string& text) {
  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    tgfx::PrintError("ReportWriter: failed to open \"%s\"", path.c_str());
    return false;
  }
  stream << text;
  return stream.good();
}

}  // namespace benchmark
