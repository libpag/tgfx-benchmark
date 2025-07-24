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

import {TGFXBind} from '../lib/tgfx';
import Benchmark from './wasm-mt/benchmark';
import {ShareData, updateSize, onresizeEvent, startDraw, setCanvasDefaultSize, setupCoordinateConversion} from "./common";

let shareData: ShareData = new ShareData();

if (typeof window !== 'undefined') {
    window.onload = async () => {
        try {
            setupCoordinateConversion('benchmark');
            shareData.BenchmarkModule = await Benchmark({
                locateFile: (file: string) => './wasm-mt/' + file,
                mainScriptUrlOrBlob: './wasm-mt/benchmark.js',
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
            throw new Error("Benchmark init failed. Please check the .wasm file path!.");
        }
    };

    window.onresize = () => {
        onresizeEvent(shareData);
        window.setTimeout(() => updateSize(shareData), 300);
    };
}
