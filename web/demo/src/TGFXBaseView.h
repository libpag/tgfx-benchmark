/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <emscripten/bind.h>
#include "base/AppHost.h"
#include "benchmark/ParticleBench.h"
#include "tgfx/gpu/opengl/webgl/WebGLWindow.h"
namespace benchmark {

class TGFXBaseView {
 public:
  TGFXBaseView(const std::string& canvasID);

  void setImagePath(const std::string& imagePath);

  void updateSize(float devicePixelRatio);

  void startDraw();

  void draw();

  void restartDraw() const;

  void updatePerfInfo(const PerfData& data);

  void updateDrawParam(const DrawParam& drawParam) const;

  void updateGraphicType(int type);

  void notifyWebUpdateGraphicType();

  ParticleBench* getBenchByIndex() const;

  void showSideBar(bool show);

  int drawIndex = 0;
  std::shared_ptr<benchmark::AppHost> appHost = nullptr;
  bool showSideBarFlag = false;

 private:
  std::shared_ptr<tgfx::Window> window = nullptr;
  std::string canvasID = "";
};

}  // namespace benchmark
