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
import Benchmark from './webgl/wasm/benchmark';
import {
    ShareData,
    appendRunNonce,
    configureBenchmarkFromQuery,
    configureBenchmarkRunFromQuery,
    configureRuntimeEnvironment,
    installBenchmarkMeasurement,
    loadBenchmarkImage,
    updateSize,
    onresizeEvent,
    startDraw,
    setCanvasDefaultSize
} from "./common";

installBenchmarkMeasurement();

let shareData: ShareData = new ShareData();

if (typeof window !== 'undefined') {
    window.onload = async () => {
        try {
            configureRuntimeEnvironment();
            shareData.BenchmarkModule = await Benchmark({
                locateFile: (file: string) => appendRunNonce('./webgl/wasm/' + file),
            });
            TGFXBind(shareData.BenchmarkModule);
            let tgfxView = shareData.BenchmarkModule.TGFXView.MakeFrom('#benchmark');
            shareData.tgfxBaseView = tgfxView;
            await loadBenchmarkImage(shareData);
            configureBenchmarkFromQuery(shareData);
            const benchmarkRun = configureBenchmarkRunFromQuery(shareData);
            setCanvasDefaultSize(
                shareData,
                benchmarkRun?.viewportWidth ?? Number(new URLSearchParams(location.search).get('width') || 2048),
                benchmarkRun?.viewportHeight ?? Number(new URLSearchParams(location.search).get('height') || 1440),
            );
            if (benchmarkRun !== null) {
                if (benchmarkRun.testIntent === 'cold-start') {
                    (window as any).__tgfxBenchmarkColdStartStartedAt = performance.now();
                    shareData.tgfxBaseView.startColdStartBenchmarkRun(
                        benchmarkRun.coldStartInitialWindowFrames,
                    );
                } else if (benchmarkRun.adaptive) {
                    shareData.tgfxBaseView.startAdaptiveBenchmarkRun(
                        benchmarkRun.stableWindowMs * 1000,
                        benchmarkRun.targetFps,
                    );
                } else if (benchmarkRun.manual) {
                    shareData.tgfxBaseView.startManualBenchmarkRun();
                } else {
                    shareData.tgfxBaseView.startBenchmarkRun(
                        benchmarkRun.measurementMs * 1000,
                    );
                }
            }
            startDraw(shareData);
            (window as any).__tgfxBenchmarkReady?.();

        } catch (error) {
            (window as any).__tgfxBenchmarkInitializationFailed?.(error);
            console.error(error);
            throw error;
        }
    };

    window.onresize = () => {
        onresizeEvent(shareData);
        window.setTimeout(() => updateSize(shareData), 300);
    };
}
