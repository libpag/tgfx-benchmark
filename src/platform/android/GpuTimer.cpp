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

#include "core/GpuTimer.h"

namespace benchmark {

/// GLES has no core timer query; it is only available through EXT_disjoint_timer_query, which
/// many drivers expose but not all. Until that path is implemented the driver reports the
/// synchronised submit upper bound instead.
std::unique_ptr<GpuTimer> CreateGpuTimer() {
  return nullptr;
}

}  // namespace benchmark
