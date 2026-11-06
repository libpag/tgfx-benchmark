#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace benchmark {

struct NativeFrameSample {
  uint64_t frameId = 0;
  int64_t callbackBeginUs = 0;
  int64_t callbackEndUs = 0;
  int64_t sceneDrawUs = 0;
  int64_t flushUs = 0;
  int64_t submitUs = 0;
};

class NativeBenchmarkState {
 public:
  void configureFromEnvironment();
  bool enabled() const;
  void start(int64_t nowUs);
  bool shouldStop(int64_t nowUs) const;
  void recordFrame(const NativeFrameSample& sample);
  std::string summaryJson() const;

 private:
  bool enabled_ = false;
  int64_t startUs_ = 0;
  int64_t warmupUs_ = 1000000;
  int64_t measurementUs_ = 1000000;
  int64_t measurementEndUs_ = 0;
  std::vector<NativeFrameSample> samples_;
};

}  // namespace benchmark
