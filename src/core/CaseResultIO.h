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
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include "core/Result.h"

namespace benchmark {

/**
 * A line oriented exchange format used to move one case result from a child process back to the
 * process that writes the final report. It is deliberately not the report JSON: the report is
 * produced by a single writer so the schema stays in one place, and the exchange format only has
 * to round trip the fields of CaseResult.
 */
class CaseResultIO {
 public:
  static bool Write(const std::string& path, const CaseResult& result);

  static bool Read(const std::string& path, CaseResult* result);
};

}  // namespace benchmark
