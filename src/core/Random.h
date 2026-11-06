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

#pragma once

#include <cstdint>

namespace benchmark {

/**
 * A fixed, tiny random number generator. The standard library distributions are intentionally not
 * used here because their output differs between implementations, and scene content must be
 * identical across platforms and compilers.
 */
class Random {
 public:
  explicit Random(uint64_t seed) : state(seed == 0 ? 0x9E3779B97F4A7C15ULL : seed) {
  }

  uint32_t nextUInt() {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    auto value = static_cast<uint32_t>((state * 0x2545F4914F6CDD1DULL) >> 32);
    return value;
  }

  /// Returns a value in [0, 1).
  float nextFloat() {
    return static_cast<float>(nextUInt() >> 8) / 16777216.0f;
  }

  float range(float lower, float upper) {
    return lower + (upper - lower) * nextFloat();
  }

  int32_t rangeInt(int32_t lower, int32_t upper) {
    if (upper <= lower) {
      return lower;
    }
    auto span = static_cast<uint32_t>(upper - lower + 1);
    return lower + static_cast<int32_t>(nextUInt() % span);
  }

 private:
  uint64_t state;
};

}  // namespace benchmark
