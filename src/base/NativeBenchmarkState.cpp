#include "NativeBenchmarkState.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace benchmark {

void NativeBenchmarkState::configureFromEnvironment() {
  const char* enabledEnv = std::getenv("TGFX_BENCH_JSON");
  enabled_ = enabledEnv != nullptr && std::string(enabledEnv) == "1";
  if (!enabled_) {
    return;
  }
  const char* warmupEnv = std::getenv("TGFX_BENCH_WARMUP_MS");
  const char* measurementEnv = std::getenv("TGFX_BENCH_MEASUREMENT_MS");
  if (warmupEnv != nullptr) {
    warmupUs_ = std::max<int64_t>(1, std::atoll(warmupEnv) * 1000);
  }
  if (measurementEnv != nullptr) {
    measurementUs_ = std::max<int64_t>(1, std::atoll(measurementEnv) * 1000);
  }
}

bool NativeBenchmarkState::enabled() const {
  return enabled_;
}

void NativeBenchmarkState::start(int64_t nowUs) {
  startUs_ = nowUs;
  measurementEndUs_ = startUs_ + warmupUs_ + measurementUs_;
  samples_.clear();
  samples_.reserve(8192);
}

bool NativeBenchmarkState::shouldStop(int64_t nowUs) const {
  return enabled_ && nowUs >= measurementEndUs_;
}

void NativeBenchmarkState::recordFrame(const NativeFrameSample& sample) {
  if (!enabled_ || samples_.size() >= 8192) {
    return;
  }
  samples_.push_back(sample);
}

std::string NativeBenchmarkState::summaryJson() const {
  std::ostringstream json;
  json << "{";
  json << "\"schemaVersion\":\"tgfx-benchmark.native-summary.v1\"";
  json << ",\"executionStatus\":\"" << (enabled_ ? "completed" : "disabled") << "\"";
  json << ",\"sampleCount\":" << samples_.size();
  json << ",\"overflowed\":" << (samples_.size() >= 8192 ? "true" : "false");
  if (!samples_.empty()) {
    json << ",\"firstSubmitUs\":" << (samples_.front().callbackEndUs - startUs_);
    json << ",\"drainAfterMeasurementUs\":"
         << std::max<int64_t>(0, samples_.back().callbackEndUs - measurementEndUs_);
  } else {
    json << ",\"firstSubmitUs\":0";
    json << ",\"drainAfterMeasurementUs\":0";
  }
  json << ",\"samples\":[";
  for (size_t i = 0; i < samples_.size(); ++i) {
    if (i != 0) {
      json << ",";
    }
    const auto& sample = samples_[i];
    json << "{"
         << "\"frameId\":" << sample.frameId << ",\"callbackBeginUs\":" << sample.callbackBeginUs
         << ",\"callbackEndUs\":" << sample.callbackEndUs
         << ",\"sceneDrawUs\":" << sample.sceneDrawUs << ",\"flushUs\":" << sample.flushUs
         << ",\"submitUs\":" << sample.submitUs << "}";
  }
  json << "]}";
  return json.str();
}

}  // namespace benchmark
