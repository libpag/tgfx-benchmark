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

#include <emscripten/bind.h>
#include "TGFXThreadsView.h"
#include "TGFXView.h"
#include "benchmark/ParticleBench.h"

using namespace benchmark;
using namespace emscripten;

EMSCRIPTEN_BINDINGS(TGFXDemo) {

  class_<TGFXBaseView>("TGFXBaseView")
      .function("setImagePath", &TGFXBaseView::setImagePath)
      .function("updateSize", &TGFXBaseView::updateSize)
      .function("startDraw", &TGFXBaseView::startDraw)
      .function("restartDraw", &TGFXBaseView::restartDraw)
      .function("updateDrawParam", &TGFXBaseView::updateDrawParam)
      .function("updateGraphicType", &TGFXBaseView::updateGraphicType)
      .function("showPerfData", &TGFXBaseView::showPerfData)
      .function("setAntiAlias", &TGFXBaseView::setAntiAlias);

  value_object<DrawParam>("DrawParam")
      .field("startCount", &DrawParam::startCount)
      .field("stepCount", &DrawParam::stepCount)
      .field("minFPS", &DrawParam::minFPS)
      .field("maxCount", &DrawParam::maxCount);

  class_<TGFXView, base<TGFXBaseView> >("TGFXView")
      .smart_ptr<std::shared_ptr<TGFXView> >("TGFXView")
      .class_function("MakeFrom", optional_override([](const std::string& canvasID) {
                        if (canvasID.empty()) {
                          return std::shared_ptr<TGFXView>(nullptr);
                        }
                        return std::make_shared<TGFXView>(canvasID);
                      }));

  class_<TGFXThreadsView, base<TGFXBaseView> >("TGFXThreadsView")
      .smart_ptr<std::shared_ptr<TGFXThreadsView> >("TGFXThreadsView")
      .class_function("MakeFrom", optional_override([](const std::string& canvasID) {
                        if (canvasID.empty()) {
                          return std::shared_ptr<TGFXThreadsView>(nullptr);
                        }
                        return std::make_shared<TGFXThreadsView>(canvasID);
                      }))
      .function("registerFonts", &TGFXThreadsView::registerFonts);
}