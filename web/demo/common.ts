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

import * as types from '../types/types';

export class TGFXBaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public init: () => void;
    public setImageSource: (image: HTMLImageElement | ImageBitmap) => boolean;
    public setNamedImageSource: (name: string, image: HTMLImageElement | ImageBitmap) => boolean;
    public updateDrawParam: (param: {
        startCount: number;
        stepCount: number;
        minFPS: number;
        maxCount: number;
    }) => void;
    public setSceneById: (stableId: string) => boolean;
    public setSceneCount: (count: number) => void;
    public setRecorderEnabled: (enabled: boolean) => void;
    public startBenchmarkRun: (measurementUs: number) => boolean;
    public startManualBenchmarkRun: () => boolean;
    public startAdaptiveBenchmarkRun: (
        stableWindowUs: number,
        targetFps: number,
    ) => boolean;
    public startColdStartBenchmarkRun: (initialWindowFrames: number) => boolean;
    public markColdStartVisible: (elapsedUs: number) => void;
    public stopBenchmarkRun: () => void;
    public isBenchmarkComplete: () => boolean;
    public getBenchmarkSummaryJson: () => string;
    public getBenchmarkLiveStatsJson: () => string;
    public showPerfData: (show: boolean) => void;
    public setAntiAlias: (enabled: boolean) => void;
    public setStroke: (enabled: boolean) => void;
    public getAOTEvidence: () => any;
}

export class ShareData {
    public BenchmarkModule: types.TGFX = null;
    public tgfxBaseView: TGFXBaseView = null;
    public resized: boolean = false;
}

interface BenchmarkState {
    initializationError: string | null;
    firstDrawAt: number | null;
    firstVisibleAt: number | null;
    readyAt: number | null;
    rampSettledAt: number | null;
    steadyStartedAt: number | null;
    animationFrames: number[];
    displayFrameIntervals: number[];
    unloadedDisplayFrameIntervals: number[];
    lastDisplayFrameAt: number | null;
    perfSamples: Array<{
        at: number;
        fps: number;
        drawTimeMs: number;
        drawCount: number;
        maxDrawCountReached: boolean;
    }>;
    lastPerf: {
        at: number;
        fps: number;
        drawTimeMs: number;
        drawCount: number;
        maxDrawCountReached: boolean;
    } | null;
    benchmarkSummary: any | null;
    benchmarkCompleteAt: number | null;
    completionPreviewDataUrl: string | null;
}

export function installBenchmarkMeasurement() {
    const state: BenchmarkState = {
        initializationError: null,
        firstDrawAt: null,
        firstVisibleAt: null,
        readyAt: null,
        rampSettledAt: null,
        steadyStartedAt: null,
        animationFrames: [],
        displayFrameIntervals: [],
        unloadedDisplayFrameIntervals: [],
        lastDisplayFrameAt: null,
        perfSamples: [],
        lastPerf: null,
        benchmarkSummary: null,
        benchmarkCompleteAt: null,
        completionPreviewDataUrl: null,
    };
    const benchmarkWindow = window as any;
    benchmarkWindow.__tgfxBenchmark = state;
    const recordInitializationError = (error: any) => {
        if (state.readyAt === null) {
            state.initializationError = error?.message || String(error);
        }
    };
    window.addEventListener('error', event => recordInitializationError(event.error || event.message));
    window.addEventListener('unhandledrejection', event => recordInitializationError(event.reason));
    benchmarkWindow.__tgfxBenchmarkInitializationFailed = recordInitializationError;
    benchmarkWindow.__tgfxBenchmarkFirstDraw = () => {
        if (state.firstDrawAt !== null) {
            return;
        }
        state.firstDrawAt = performance.now();
        requestAnimationFrame((timestamp) => {
            state.firstVisibleAt = timestamp;
            const startedAt = benchmarkWindow.__tgfxBenchmarkColdStartStartedAt;
            if (Number.isFinite(startedAt)) {
                benchmarkWindow.__tgfxBenchmarkView?.markColdStartVisible?.(
                    Math.round(Math.max(0, timestamp - startedAt) * 1000),
                );
            }
        });
    };
    benchmarkWindow.__tgfxBenchmarkReady = () => {
        if (state.readyAt === null) {
            state.readyAt = performance.now();
        }
    };
    benchmarkWindow.__tgfxBenchmarkRunComplete = (summaryJson: string) => {
        try {
            const canvas = document.getElementById('benchmark') as HTMLCanvasElement | null;
            state.completionPreviewDataUrl = canvas?.toDataURL('image/png') || null;
        } catch (error) {
            state.completionPreviewDataUrl = null;
        }
        try {
            state.benchmarkSummary = JSON.parse(summaryJson);
            state.benchmarkCompleteAt = performance.now();
        } catch (error) {
            console.error('Failed to parse benchmark summary', error);
        }
    };
    benchmarkWindow.updatePerfInfo = (
        fps: number,
        drawTimeMs: number,
        drawCount: number,
        maxDrawCountReached: boolean,
    ) => {
        const sample = {
            at: performance.now(),
            fps,
            drawTimeMs,
            drawCount,
            maxDrawCountReached,
        };
        state.lastPerf = sample;
        if (maxDrawCountReached && state.rampSettledAt === null) {
            state.rampSettledAt = sample.at;
        }
        if (state.steadyStartedAt === null) {
            return;
        }
        state.perfSamples.push(sample);
    };
    benchmarkWindow.__tgfxBenchmarkIsRamped = () => state.rampSettledAt !== null;
    benchmarkWindow.__tgfxBenchmarkStartSteady = () => {
        state.steadyStartedAt = performance.now();
        state.animationFrames = [];
        state.perfSamples = [];
    };
    benchmarkWindow.__tgfxBenchmarkDisplayFps = () => {
        if (state.displayFrameIntervals.length < 8) {
            return null;
        }
        const sorted = [...state.displayFrameIntervals].sort((a, b) => a - b);
        const median = sorted[Math.floor(sorted.length / 2)];
        return median > 0 ? 1000 / median : null;
    };
    benchmarkWindow.__tgfxBenchmarkUnloadedDisplayFps = () => {
        if (state.unloadedDisplayFrameIntervals.length < 16) return null;
        const sorted = [...state.unloadedDisplayFrameIntervals].sort((a, b) => a - b);
        const median = sorted[Math.floor(sorted.length / 2)];
        return median > 0 ? 1000 / median : null;
    };
    benchmarkWindow.__tgfxBenchmarkSnapshot = () => {
        const quantile = (values: number[], fraction: number) => {
            if (values.length === 0) {
                return null;
            }
            const sorted = [...values].sort((a, b) => a - b);
            const index = Math.min(sorted.length - 1, Math.max(0, Math.ceil(sorted.length * fraction) - 1));
            return sorted[index];
        };
        const intervals = state.animationFrames.slice(1).map((value, index) =>
            value - state.animationFrames[index]);
        const lastPerfSample = state.perfSamples.length > 0
            ? state.perfSamples[state.perfSamples.length - 1]
            : null;
        return {
            navigationToReadyMs: state.readyAt,
            navigationToFirstDrawMs: state.firstDrawAt,
            navigationToFirstVisibleMs: state.firstVisibleAt,
            rampSettledAt: state.rampSettledAt,
            steadyStartedAt: state.steadyStartedAt,
            steadyEndedAt: performance.now(),
            animationFrameCount: state.animationFrames.length,
            animationFrameIntervalP50Ms: quantile(intervals, 0.5),
            animationFrameIntervalP95Ms: quantile(intervals, 0.95),
            animationFrameIntervalP99Ms: quantile(intervals, 0.99),
            animationFrameIntervalMaxMs: intervals.length > 0 ? Math.max(...intervals) : null,
            displayRefreshFps: benchmarkWindow.__tgfxBenchmarkDisplayFps(),
            measuredRafFps: intervals.length > 0
                ? 1000 / (intervals.reduce((sum, value) => sum + value, 0) / intervals.length)
                : null,
            perfSampleCount: state.perfSamples.length,
            benchmarkFpsLast: lastPerfSample?.fps ?? null,
            benchmarkDrawTimeMsP50: quantile(state.perfSamples.map((sample) => sample.drawTimeMs), 0.5),
            benchmarkDrawTimeMsP95: quantile(state.perfSamples.map((sample) => sample.drawTimeMs), 0.95),
            benchmarkDrawTimeMsP99: quantile(state.perfSamples.map((sample) => sample.drawTimeMs), 0.99),
            drawCountLast: lastPerfSample?.drawCount ?? null,
            rampLastPerf: state.lastPerf,
            config: benchmarkWindow.__tgfxBenchmarkConfig ?? null,
            perfSamples: state.perfSamples,
            animationFrames: state.animationFrames,
        };
    };
    const collectAnimationFrame = (timestamp: number) => {
        if (state.lastDisplayFrameAt !== null && timestamp > state.lastDisplayFrameAt) {
            const interval = timestamp - state.lastDisplayFrameAt;
            state.displayFrameIntervals.push(interval);
            if (state.firstDrawAt === null) {
                state.unloadedDisplayFrameIntervals.push(interval);
            }
            if (state.displayFrameIntervals.length > 180) {
                state.displayFrameIntervals.shift();
            }
        }
        state.lastDisplayFrameAt = timestamp;
        if (state.steadyStartedAt !== null) {
            state.animationFrames.push(timestamp);
        }
        requestAnimationFrame(collectAnimationFrame);
    };
    requestAnimationFrame(collectAnimationFrame);
}

export function appendRunNonce(url: string): string {
    const nonce = new URLSearchParams(window.location.search).get('runNonce');
    if (!nonce) {
        return url;
    }
    const separator = url.includes('?') ? '&' : '?';
    return `${url}${separator}runNonce=${encodeURIComponent(nonce)}`;
}

export function updateSize(shareData: ShareData) {
    if (!shareData.tgfxBaseView) {
        return;
    }
    shareData.resized = false;
    let canvas = document.getElementById('benchmark') as HTMLCanvasElement;
    let container = document.getElementById('container') as HTMLDivElement;
    let screenRect = container.getBoundingClientRect();
    let scaleFactor = window.devicePixelRatio;
    canvas.width = screenRect.width * scaleFactor;
    canvas.height = screenRect.height * scaleFactor;
    canvas.style.width = screenRect.width + "px";
    canvas.style.height = screenRect.height + "px";
    shareData.tgfxBaseView.updateSize(scaleFactor);
}

export function startDraw(shareData: ShareData) {
    if (!shareData.tgfxBaseView) {
        return;
    }
    shareData.tgfxBaseView.startDraw();
}

export async function loadBenchmarkImage(shareData: ShareData) {
    const images: Array<[string, string]> = [
        ['bridge', './static/image/bridge.jpg'],
        ['motionmark.compass', './static/image/motionmark/compass100.png'],
        ['motionmark.console', './static/image/motionmark/console100.png'],
        ['motionmark.contribute', './static/image/motionmark/contribute100.png'],
        ['motionmark.debugger', './static/image/motionmark/debugger100.png'],
        ['motionmark.inspector', './static/image/motionmark/inspector100.png'],
        ['motionmark.layout', './static/image/motionmark/layout100.png'],
        ['motionmark.performance', './static/image/motionmark/performance100.png'],
        ['motionmark.script', './static/image/motionmark/script100.png'],
        ['motionmark.shortcuts', './static/image/motionmark/shortcuts100.png'],
        ['motionmark.standards', './static/image/motionmark/standards100.png'],
        ['motionmark.storage', './static/image/motionmark/storage100.png'],
        ['motionmark.styles', './static/image/motionmark/styles100.png'],
        ['motionmark.timeline', './static/image/motionmark/timeline100.png'],
    ];
    await Promise.all(images.map(async ([name, src]) => {
        const image = new Image();
        image.src = appendRunNonce(src);
        await image.decode();
        if (!shareData.tgfxBaseView.setNamedImageSource(name, image)) {
            throw new Error(`TGFX could not load benchmark image: ${name}`);
        }
    }));
}

export function benchmarkSceneVersion(caseId: string) {
    return caseId === 'coldstart.shader-mix' ? 'v7' :
        caseId.startsWith('motionmark.') ? 'v2' : null;
}

export function configureBenchmarkFromQuery(shareData: ShareData) {
    const params = new URLSearchParams(window.location.search);
    const benchmarkMode = params.get('benchmark') === '1';
    const caseId = params.get('case') || 'motionmark.multiply';
    const count = benchmarkCount(params, caseId, 512);
    const ramp = params.get('ramp') === '1';
    const aotRequested = params.get('aot') !== '0';
    const particleOptions = applyParticleOptionsFromQuery(shareData, params);
    (window as any).__tgfxBenchmarkConfig = {
        caseId,
        sceneVersion: benchmarkSceneVersion(caseId),
        count: ramp ? null : count,
        ramp,
        aotRequested,
        backend: params.get('backend') || null,
        runNonce: params.get('runNonce') || null,
        antialias: particleOptions.antialias,
        stroke: particleOptions.stroke,
        url: window.location.href,
    };
    (window as any).__tgfxBenchmarkView = shareData.tgfxBaseView;
    if (!shareData.tgfxBaseView.setSceneById(caseId)) {
        throw new Error(`Unsupported benchmark case: ${caseId}`);
    }
    shareData.tgfxBaseView.setSceneCount(count);
    if (!ramp && !benchmarkMode) {
        shareData.tgfxBaseView.updateDrawParam({
            startCount: count,
            stepCount: 0,
            minFPS: 60,
            maxCount: count,
        });
    }
    shareData.tgfxBaseView.showPerfData(params.get('overlay') === '1');
}

export interface BenchmarkRunOptions {
    caseId: string;
    count: number;
    measurementMs: number | null;
    manual: boolean;
    adaptive: boolean;
    testIntent: 'cold-start' | 'steady-state';
    coldStartInitialWindowFrames: number;
    targetFps: number;
    stableWindowMs: number;
    viewportWidth: number;
    viewportHeight: number;
    runNonce: string;
    antialias: boolean | null;
    stroke: boolean | null;
}

export function configureBenchmarkRunFromQuery(shareData: ShareData): BenchmarkRunOptions | null {
    const params = new URLSearchParams(window.location.search);
    if (params.get('benchmark') !== '1') {
        return null;
    }
    const caseId = params.get('case') || 'motionmark.multiply';
    const count = benchmarkCount(params, caseId, 512);
    const requestedIntent = params.get('testIntent') || 'steady-state';
    if (requestedIntent !== 'cold-start' && requestedIntent !== 'steady-state') {
        throw new Error(`Unsupported testIntent: ${requestedIntent}`);
    }
    const coldStart = requestedIntent === 'cold-start';
    if (coldStart && (params.get('manual') === '1' || params.get('adaptive') === '1')) {
        throw new Error('cold-start does not accept manual or adaptive run modes');
    }
    const manual = !coldStart && params.get('manual') === '1';
    const adaptive = !coldStart && params.get('adaptive') === '1';
    const targetFps = Math.max(1, Number(params.get('targetFps') || 60));
    const stableWindowMs = Math.max(
        100,
        Number(params.get('stableMs') || params.get('measurementMs') || 3000),
    );
    const measurementMs = coldStart ? null : manual
        ? null
        : Math.max(1, Number(params.get('measurementMs') || 10000));
    const coldStartInitialWindowFrames = Math.max(
        1,
        Number(params.get('initialWindowFrames') || 30),
    );
    const viewportWidth = Math.max(1, Number(params.get('width') || 900));
    const viewportHeight = Math.max(1, Number(params.get('height') || 600));
    const runNonce = params.get('runNonce') || '';
    const particleOptions = applyParticleOptionsFromQuery(shareData, params);

    if (!shareData.tgfxBaseView.setSceneById(caseId)) {
        throw new Error(`Unsupported benchmark case: ${caseId}`);
    }
    shareData.tgfxBaseView.setSceneCount(count);
    shareData.tgfxBaseView.setRecorderEnabled(params.get('recorder') !== '0');
    shareData.tgfxBaseView.showPerfData(false);
    return {
        caseId,
        count,
        measurementMs,
        manual,
        adaptive,
        testIntent: requestedIntent,
        coldStartInitialWindowFrames,
        targetFps,
        stableWindowMs,
        viewportWidth,
        viewportHeight,
        runNonce,
        ...particleOptions,
    };
}

function applyParticleOptionsFromQuery(
    shareData: ShareData,
    params: URLSearchParams,
): {antialias: boolean | null, stroke: boolean | null} {
    const antialias = parseBoolean(params.get('aa') ?? params.get('antialias'));
    const stroke = parseBoolean(params.get('stroke'));
    if (antialias !== null) {
        shareData.tgfxBaseView.setAntiAlias(antialias);
    }
    if (stroke !== null) {
        shareData.tgfxBaseView.setStroke(stroke);
    }
    return {antialias, stroke};
}

function parseBoolean(value: string | null): boolean | null {
    if (value === '1' || value === 'true') {
        return true;
    }
    if (value === '0' || value === 'false') {
        return false;
    }
    return null;
}

function benchmarkCount(params: URLSearchParams, caseId: string | null, fallback: number) {
    const requested = Number(params.get('count'));
    if (Number.isSafeInteger(requested) && requested > 0) {
        return Math.min(1000000, requested);
    }
    const sceneDefaults: Record<string, number> = {
        'coldstart.shader-mix': 256,
        'motionmark.multiply': 512,
        'motionmark.canvas-arcs': 8,
        'motionmark.leaves': 128,
        'motionmark.canvas-lines': 16,
        'motionmark.images': 64,
        'motionmark.design': 8,
        'motionmark.suits': 24,
    };
    return caseId && sceneDefaults[caseId] ? sceneDefaults[caseId] : fallback;
}

export function configureRuntimeEnvironment() {
    const params = new URLSearchParams(window.location.search);
    const env: Record<string, string> = {};
    if (params.get('aot') === '0') {
        env.TGFX_AOT_DISABLE = '1';
    }
    if (params.has('aot_diag')) {
        env.TGFX_AOT_DIAG = '1';
    }
    (globalThis as any).__TGFX_BENCH_ENV__ = env;
}

export function onresizeEvent(shareData: ShareData) {
    if (!shareData.tgfxBaseView) {
        return;
    }
    shareData.resized = true;
}

const canvasDefaultWidth:number = 2048;
const canvasDefaultHeight:number = 1440;

export function setCanvasDefaultSize(
    shareData: ShareData,
    width: number = canvasDefaultWidth,
    height: number = canvasDefaultHeight,
) {
    const container = document.getElementById('container') as HTMLDivElement;
    const canvas = document.getElementById('benchmark') as HTMLCanvasElement;
    const fluid = new URLSearchParams(location.search).get('fluid') === '1';

    container.style.backgroundColor = '#2c2c2c';
    canvas.style.border = window.parent === window && !fluid ? '10px solid #ffffff' : '0';
    canvas.style.boxSizing = 'content-box';
    canvas.style.backgroundColor = '#ffffff';
    const scaleFactor = window.devicePixelRatio;
    if (fluid) {
        const rect = container.getBoundingClientRect();
        canvas.width = Math.max(1, Math.round(rect.width * scaleFactor));
        canvas.height = Math.max(1, Math.round(rect.height * scaleFactor));
        canvas.style.width = rect.width + 'px';
        canvas.style.height = rect.height + 'px';
        shareData.tgfxBaseView.updateSize(scaleFactor);
        return;
    }
    canvas.width = width;
    canvas.height = height;

    const scaleWidth = canvas.width / scaleFactor;
    const scaleHeight = canvas.height / scaleFactor;

    const embedded = window.parent !== window;
    canvas.style.width = (embedded ? width : scaleWidth) + 'px';
    canvas.style.height = (embedded ? height : scaleHeight) + 'px';
    container.style.display = 'flex';
    container.style.justifyContent = 'center';
    container.style.alignItems = 'center';
    shareData.tgfxBaseView.updateSize(scaleFactor);
}
