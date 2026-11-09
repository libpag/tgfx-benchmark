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

#include "core/Process.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <spawn.h>
#include <sys/wait.h>
#include <vector>
extern char** environ;
#endif

namespace benchmark {

std::string ExecutablePath() {
#if defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  if (size == 0) {
    return {};
  }
  std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) != 0) {
    return {};
  }
  while (!path.empty() && path.back() == '\0') {
    path.pop_back();
  }
  return path;
#else
  return {};
#endif
}

int RunProcess(const std::vector<std::string>& arguments) {
#if defined(__APPLE__)
  if (arguments.empty()) {
    return -1;
  }
  std::vector<char*> argv;
  argv.reserve(arguments.size() + 1);
  for (const auto& argument : arguments) {
    argv.push_back(const_cast<char*>(argument.c_str()));
  }
  argv.push_back(nullptr);

  pid_t child = 0;
  auto status = posix_spawn(&child, argv[0], nullptr, nullptr, argv.data(), environ);
  if (status != 0) {
    return -1;
  }
  int waitStatus = 0;
  if (waitpid(child, &waitStatus, 0) < 0) {
    return -1;
  }
  if (WIFEXITED(waitStatus)) {
    return WEXITSTATUS(waitStatus);
  }
  return -1;
#else
  (void)arguments;
  return -1;
#endif
}

}  // namespace benchmark
