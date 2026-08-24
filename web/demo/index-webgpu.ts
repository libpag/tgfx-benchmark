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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import {TGFXBind} from '../lib/tgfx';
import Benchmark from './webgpu/wasm-mt/benchmark';
import {ShareData, updateSize, onresizeEvent, startDraw, setCanvasDefaultSize, setupCoordinateConversion} from "./common";

let shareData: ShareData = new ShareData();

if (typeof window !== 'undefined') {
    window.onload = async () => {
        try {
            if (!navigator.gpu) {
                throw new Error("WebGPU is not supported in this browser.");
            }
            const adapter = await navigator.gpu.requestAdapter();
            if (!adapter) {
                throw new Error("Failed to get WebGPU adapter.");
            }
            const device = await adapter.requestDevice();

            setupCoordinateConversion('benchmark');
            shareData.BenchmarkModule = await Benchmark({
                locateFile: (file: string) => './webgpu/wasm-mt/' + file,
                mainScriptUrlOrBlob: './webgpu/wasm-mt/benchmark.js',
                preinitializedWebGPUDevice: device,
            });
            TGFXBind(shareData.BenchmarkModule);

            let tgfxView = shareData.BenchmarkModule.TGFXThreadsView.MakeFrom('#benchmark');
            shareData.tgfxBaseView = tgfxView;
            var imagePath = "../../resources/assets/bridge.jpg";
            await tgfxView.setImagePath(imagePath);

            var fontPath = "../../resources/font/NotoSansSC-Regular.otf";
            const fontBuffer = await fetch(fontPath).then((response) => response.arrayBuffer());
            const fontUIntArray = new Uint8Array(fontBuffer);
            var emojiFontPath = "../../resources/font/NotoColorEmoji.ttf";
            const emojiFontBuffer = await fetch(emojiFontPath).then((response) => response.arrayBuffer());
            const emojiFontUIntArray = new Uint8Array(emojiFontBuffer);
            tgfxView.registerFonts(fontUIntArray, emojiFontUIntArray);
            setCanvasDefaultSize(shareData);
            startDraw(shareData);
        } catch (error) {
            console.error(error);
            throw new Error("Benchmark WebGPU initialization failed.");
        }
    };

    window.onresize = () => {
        onresizeEvent(shareData);
        window.setTimeout(() => updateSize(shareData), 300);
    };
}
