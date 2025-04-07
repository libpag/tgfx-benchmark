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
import {
    shareData,
    updateSize,
    onresizeEvent,
    pageInit,
    getEngineDir,
    loadModule,
    bindEventListeners,
    setupCoordinateConversion
} from "./common";


if (typeof window !== 'undefined') {
    window.onload = async () => {
        pageInit();
        const isSupported = JSON.parse(localStorage.getItem('isSupported'));
        if (isSupported) {
            setupCoordinateConversion('benchmark',true);
            const engineDir = getEngineDir();
            if (engineDir === "") {
                throw "engineDir is None";
            }
            const threadTypeSelect = document.getElementById('thread-type-select') as HTMLSelectElement;

            await loadModule(engineDir,threadTypeSelect.value.toString());
            bindEventListeners();

        } else {
            throw "This website only supports desktop browsers based on Chromium (like Chrome or Edge). Please switch to one of these browsers to access it.";
        }
    };
    window.onresize = () => {
        const isSupported = JSON.parse(localStorage.getItem('isSupported'));
        if (isSupported) {
            onresizeEvent(shareData);
            window.setTimeout(() => updateSize(shareData), 300);
        }
    };
}