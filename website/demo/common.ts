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
import * as types from '../types/types';
import {TGFXBind} from '../lib/tgfx';

class DrawParam {
    startCount: number = 1;
    stepCount: number = 1000;
    minFPS: number = 60.0;
    maxCount: number = 1000000;
}

let drawParam: DrawParam = new DrawParam();
let drawIndex: number = 0;

class BaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public restartDraw: () => void;
    public updateDrawParam: (drawParam: DrawParam) => void;
    public updateGraphicType: (type: number) => void;
    public showPerfData: (status: boolean) => void;
    public init: () => void;
}

export class TGFXBaseView extends BaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public restartDraw: () => void;
    public updateDrawParam: (drawParam: DrawParam) => void;
    public updateGraphicType: (type: number) => void;
    public showPerfData: (status: boolean) => void;
    public init: () => void;
}

export class SkiaView extends BaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public restartDraw: () => void;
    public updateDrawParam: (drawParam: DrawParam) => void;
    public updateGraphicType: (type: number) => void;
    public showPerfData: (status: boolean) => void;
    public init: () => void;
}

export class ShareData {
    public BenchmarkModule: types.TGFX = null;
    public baseView: BaseView = null;
    public resized: boolean = false;
    public init: () => void;
}

export let shareData: ShareData;

enum EnumEngineType {
    tgfx = "tgfx",
    skia = "skia"
}

export let enumEngineType: EnumEngineType;


export enum GraphicType {
    Rect = 0,
    Circle = 1,
    Oval = 2,
    RRect = 3,
    Star = 4
}


export const graphicTypeStr: readonly ['Rect', 'Circle', 'Oval', 'RRect'] = ['Rect', 'Circle', 'Oval', 'RRect'];

export let engineVersionInfo: SideBarConfig;

export interface ParamsObject {
    [key: string]: string | number;
}

export let defaultUrlParams: ParamsObject = {
    engine: "tgfx",
    version: '',
    start: 1,
    step: 600,
    max: 1000000,
    min: 60,
    graphic: "rect",
};

export function updateSize(shareData: ShareData) {
    if (!shareData.baseView) {
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
    shareData.baseView.updateSize(scaleFactor);

    const resolutionSpan = document.querySelector('.resolution');
    resolutionSpan.textContent = `${canvas.width}x${canvas.height}`;
}

export function startDraw(shareData: ShareData) {
    if (!shareData.baseView) {
        return;
    }
    shareData.baseView.startDraw();
}

export function onresizeEvent(shareData: ShareData) {
    if (!shareData.baseView) {
        return;
    }
    shareData.resized = true;
}

interface EngineConfig {
    savePath: string;
    versions: {
        [version: string]: string[];
    };
}

interface SideBarConfig {
    engineVersion: {
        tgfx?: EngineConfig;
        skia?: EngineConfig;
    };
}

function getResourceUrl(): string {
    const isLocalhost = window.location.hostname === 'localhost' ||
        window.location.hostname === '127.0.0.1';

    return isLocalhost ? '' : 'https://pag.qq.com/tgfx/benchmark';
}

export function parseSideBarParam(): SideBarConfig {
    try {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', `${getResourceUrl()}/static/sideBarConfigParam.json`, false);
        xhr.send();
        const config = JSON.parse(xhr.responseText) as SideBarConfig;
        if (!config.engineVersion) {
            throw new Error('Configuration file format error: engineVersion configuration item is missing');
        }

        for (const engine of ['tgfx', 'skia'] as const) {
            const engineConfig = config.engineVersion[engine];
            if (engineConfig) {
                if (!engineConfig.savePath) {
                    throw new Error(`Configuration file format error: ${engine.toUpperCase()} missing savePath`);
                }
                if (!engineConfig.versions || Object.keys(engineConfig.versions).length === 0) {
                    throw new Error(`Configuration file format error: ${engine.toUpperCase()} missing versionInfo`);
                }

                for (const [version, threadTypes] of Object.entries(engineConfig.versions)) {
                    if (!Array.isArray(threadTypes) || threadTypes.length === 0) {
                        throw new Error(`Configuration file format error: the thread type configuration of ${engine.toUpperCase()} in ${version} is empty`);
                    }
                    if (!threadTypes.every(type => type === 'st' || type === 'mt')) {
                        throw new Error(`Configuration file format error: the thread type configuration of ${engine.toUpperCase()} in ${version} is invalid`);
                    }
                }
            }
        }

        if (!config.engineVersion.tgfx && !config.engineVersion.skia) {
            throw new Error('Configuration file format error: at least one engine configuration is required');
        }
        engineVersionInfo = config;
        return config;
    } catch (error) {
        throw new Error(`Failed to parse sidebar configuration file: ${error}`);
    }
}

export function pageInit() {
    const needRestore = localStorage.getItem('needRestore') === 'true';

    if (!needRestore) {
        setDefaultValues();
    }
    const config = parseSideBarParam();
    const versionSelect = document.getElementById('engine-version') as HTMLSelectElement;
    let param: ParamsObject = {};
    if (window.location.search !== '') {
        const urlParams = UrlParamsManager.getUrlParams();
        if (!UrlParamsManager.validateUrlParams(urlParams)) {
            UrlParamsManager.clearUrlParams();
        }
    }

    let queryString = window.location.search;

    function handleEngineVisibility() {
        const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
        if (!engineTypeSelect) return;

        engineTypeSelect.innerHTML = '';

        if (config.engineVersion.tgfx) {
            const tgfxOption = document.createElement('option');
            tgfxOption.value = 'tgfx';
            tgfxOption.textContent = 'tgfx';
            engineTypeSelect.appendChild(tgfxOption);
        }

        if (config.engineVersion.skia) {
            const skiaOption = document.createElement('option');
            skiaOption.value = 'skia';
            skiaOption.textContent = 'skia';
            engineTypeSelect.appendChild(skiaOption);
        }
        if (needRestore) {
            restoreEngineType();
            return;
        }
        if (queryString !== '') {
            param = UrlParamsManager.getUrlParams();

            if (param['engine']) {
                let type = param['engine'].toString().toLowerCase();
                if (type === 'tgfx' || type === 'skia') {
                    engineTypeSelect.value = type;
                    return;
                }
            }
        }
        if (engineTypeSelect.options.length > 0) {
            engineTypeSelect.value = engineTypeSelect.options[0].value;
        }
    }

    function updateVersionSelect(engineType: string) {
        if (!versionSelect) return;
        if (needRestore) return;

        versionSelect.innerHTML = '';

        const engineConfig = config.engineVersion[engineType as keyof typeof config.engineVersion];
        if (engineConfig) {
            Object.keys(engineConfig.versions).forEach(version => {
                const option = document.createElement('option');
                option.value = version;
                option.textContent = version;
                versionSelect.appendChild(option);
            });
        }
    }


    handleEngineVisibility();

    const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
    if (engineTypeSelect) {
        updateVersionSelect(engineTypeSelect.value);
    }

    if (engineTypeSelect) {
        engineTypeSelect.addEventListener('change', (e) => {
            const target = e.target as HTMLSelectElement;
            updateVersionSelect(target.value);
        });
    }

    if (localStorage.getItem('needRestore') === 'true') {
        restorePageSettings();
    }
    if (queryString !== '') {
        try {
            param = UrlParamsManager.getUrlParams();
            if (param['engine']) {
                engineTypeSelect.value = param['engine'].toString().toLowerCase();
            }
            if (param['version']) {
                const targetVersion = param['version'].toString();
                const matchingOption = Array.from(versionSelect.options).find(option =>
                    option.value.toLowerCase() === targetVersion.toLowerCase()
                );
                if (matchingOption) {
                    versionSelect.value = matchingOption.value;
                }
            }
            const configInputs = document.querySelectorAll('#config-param input[type="number"]');

            configInputs.forEach(input => {
                const element = input as HTMLInputElement;
                let key: string = htmlParamConvertUrlParam(element.id);
                if (key in param && param[key] !== undefined) {
                    const value = Number(param[key]);
                    if (typeof value === 'number') {
                        element.value = value.toString();
                    } else {
                        console.warn(`The value of ${key} is not of numeric type`);
                    }
                }
            });

            if (param['graphic']) {
                let graphicType = param['graphic'].toString().toLowerCase();
                const selectedInput = Array.from<HTMLInputElement>(document.querySelectorAll('input[name="graphic-type"]'))
                    .find(input => input.value.toLowerCase() === graphicType.toLowerCase()) as HTMLInputElement;

                if (selectedInput) {
                    selectedInput.checked = true;
                }
            }

        } catch (error) {
            throw new Error(`The url parameter is incorrect: ${error}`);
        }
    }
    if (engineTypeSelect.value === 'tgfx') {
        enumEngineType = EnumEngineType.tgfx;
    } else if (engineTypeSelect.value === 'skia') {
        enumEngineType = EnumEngineType.skia;
    }
}


interface PageSettings {
    engineType: {
        options: string[];
        selected: string;
    };
    engineVersion: {
        options: string[];
        selected: string;
    };
    configParams: {
        startCount: number;
        stepCount: number;
        maxShapes: number;
        minFPS: number;
    };
    graphicType: {
        options: string[];
        selected: string;
    };
    language: {
        options: string[];
        selected: string;
    };
}

function getPageSettings(): PageSettings {
    const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
    const engineType = {
        options: Array.from(engineTypeSelect.options).map(option => option.value),
        selected: engineTypeSelect.value
    };

    const versionSelect = document.getElementById('engine-version') as HTMLSelectElement;
    const engineVersion = {
        options: Array.from(versionSelect.options).map(option => option.value),
        selected: versionSelect.value
    };

    const configParams = {
        startCount: Number((document.getElementById('startCount') as HTMLInputElement).value),
        stepCount: Number((document.getElementById('stepCount') as HTMLInputElement).value),
        maxShapes: Number((document.getElementById('maxShapes') as HTMLInputElement).value),
        minFPS: Number((document.getElementById('minFPS') as HTMLInputElement).value)
    };

    const graphicTypeInputs = document.getElementsByName('graphic-type') as NodeListOf<HTMLInputElement>;
    const graphicType = {
        options: Array.from(graphicTypeInputs).map(input => input.value),
        selected: Array.from(graphicTypeInputs).find(input => input.checked)?.value || ''
    };

    const languageSelect = document.getElementById('language-type') as HTMLSelectElement;
    const language = {
        options: Array.from(languageSelect.options).map(option => option.value),
        selected: languageSelect.value
    };

    return {
        engineType,
        engineVersion,
        configParams,
        graphicType,
        language
    };
}

export function pageRestart() {
    const settings = getPageSettings();

    localStorage.setItem('pageSettings', JSON.stringify(settings));
    localStorage.setItem('needRestore', 'true');
    location.reload();
}

function restoreEngineType(): void {
    const savedSettings = localStorage.getItem('pageSettings');
    if (!savedSettings) {
        return;
    }

    let settings: PageSettings;
    try {
        settings = JSON.parse(savedSettings);
    } catch (error) {
        console.error('Page parameter parsing failed:', error);
        return;
    }
    const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
    if (engineTypeSelect && settings.engineType.selected) {
        engineTypeSelect.value = settings.engineType.selected;
    }
}

export function restorePageSettings(): void {
    const savedSettings = localStorage.getItem('pageSettings');
    if (!savedSettings) {
        return;
    }

    let settings: PageSettings;
    try {
        settings = JSON.parse(savedSettings);
    } catch (error) {
        console.error('Page parameter parsing failed:', error);
        return;
    }

    try {
        let param: ParamsObject = {};
        const versionSelect = document.getElementById('engine-version') as HTMLSelectElement;
        if (versionSelect && settings.engineVersion.options.length > 0) {
            versionSelect.innerHTML = '';

            settings.engineVersion.options.forEach(version => {
                const option = document.createElement('option');
                option.value = version;
                option.textContent = version;
                versionSelect.appendChild(option);
            });

            if (settings.engineVersion.selected) {
                versionSelect.value = settings.engineVersion.selected;
                versionSelect.dispatchEvent(new Event('change'));
                param["version"] = settings.engineVersion.selected;
            }
        }

        const {configParams} = settings;
        const inputs = {
            startCount: document.getElementById('startCount') as HTMLInputElement,
            stepCount: document.getElementById('stepCount') as HTMLInputElement,
            maxShapes: document.getElementById('maxShapes') as HTMLInputElement,
            minFPS: document.getElementById('minFPS') as HTMLInputElement
        };

        Object.entries(inputs).forEach(([key, input]) => {
            if (input && configParams[key as keyof typeof configParams] !== undefined) {
                input.value = configParams[key as keyof typeof configParams].toString();
            }
        });

        const graphicTypeRadio = document.querySelector(
            `input[name="graphic-type"][value="${settings.graphicType.selected}"]`
        ) as HTMLInputElement;
        if (graphicTypeRadio) {
            graphicTypeRadio.checked = true;
        }
        const languageSelect = document.getElementById('language-type') as HTMLSelectElement;
        if (languageSelect && settings.language.selected) {
            languageSelect.value = settings.language.selected;
        }

    } catch (error) {
        console.error('Failed to restore page configuration:', error);
    }
}


function setDefaultValues() {
    const mtRadio = document.querySelector('input[name="thread-type"][value="mt"]') as HTMLInputElement;
    if (mtRadio) {
        mtRadio.checked = true;
    }
    const defaultParams = {
        startCount: 1,
        stepCount: 1000,
        maxShapes: 1000000,
        minFPS: 60.0
    };

    Object.entries(defaultParams).forEach(([key, value]) => {
        const input = document.getElementById(key) as HTMLInputElement;
        if (input) {
            input.value = value.toString();
        }
    });
    const rectangleRadio = document.querySelector('input[name="graphic-type"][value="Rect"]') as HTMLInputElement;
    if (rectangleRadio) {
        rectangleRadio.checked = true;
    }
    const languageSelect = document.getElementById('language-type') as HTMLSelectElement;
    if (languageSelect) {
        languageSelect.value = 'auto';
    }
}


export function getEngineDir(): string {
    const needRestore = localStorage.getItem('needRestore') === 'true';
    if (!needRestore) {
        const versionSelect = document.getElementById('engine-version') as HTMLSelectElement;
        const version = versionSelect.value;
        const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
        if (engineTypeSelect.value === 'tgfx') {
            return `${engineVersionInfo.engineVersion.tgfx.savePath}benchmark-${version}`;
        } else if (engineTypeSelect.value === 'skia') {
            return `${engineVersionInfo.engineVersion.skia.savePath}benchmark-${version}`;
        }
    } else {
        const engineDir = localStorage.getItem('engineDir');
        localStorage.removeItem('needRestore');
        if (engineDir) {
            return engineDir;
        }
    }
    return '';
}


function handleConfigParamChange(event: Event) {
    const target = event.target as HTMLInputElement;
    let param: ParamsObject = {};
    if (target.type !== 'number') {
        return;
    }
    if (!event.isTrusted) {
        return;
    }
    const minValue = parseFloat(target.min);
    const currentValue = parseFloat(target.value);

    if (currentValue < minValue) {
        target.value = minValue.toString();
    }
    param[htmlParamConvertUrlParam(target.id)] = Number(target.value);
    if (target.id === 'startCount') {
        drawParam.startCount = Number(target.value);
    } else if (target.id === 'stepCount') {
        drawParam.stepCount = Number(target.value);
    } else if (target.id === 'maxShapes') {
        drawParam.maxCount = Number(target.value);
    } else if (target.id === 'minFPS') {
        drawParam.minFPS = Number(target.value);
    } else {
        console.error('Unknown parameter type:', target.id);
        return;
    }
    shareData.baseView.updateDrawParam(drawParam);
    UrlParamsManager.setUrlParams(param);
}

function setGraphicType(graphicType: string) {
    graphicType = graphicType.toLowerCase();
    let type: GraphicType;
    switch (graphicType) {
        case 'rect':
            type = GraphicType.Rect;
            break;
        case 'circle':
            type = GraphicType.Circle;
            break;
        case 'rrect':
            type = GraphicType.RRect;
            break;
        case 'oval':
            type = GraphicType.Oval;
            break;
        case 'star':
            type = GraphicType.Star;
            break;
        default:
            type = GraphicType.Rect;
    }
    if (shareData.baseView) {
        drawIndex = type;
        shareData.baseView.updateGraphicType(type);
    }
}

function handleGraphicTypeChange(event: Event) {
    const target = event.target as HTMLInputElement;
    let param: ParamsObject = {};

    if (target.type === 'radio' && target.name === 'graphic-type' && target.checked) {

        let graphicType = target.value;
        param["graphic"] = graphicType;
        UrlParamsManager.setUrlParams(param);
        if (event.isTrusted) {
            setGraphicType(graphicType);
        }
    }
}

function handleEngineTypeChange(event: Event) {
    if (!event.isTrusted) {
        return;
    }
    const target = event.target as HTMLSelectElement;
    let engineDir = "";
    const selectedEngine = target.value;
    const engineVersionSelect = document.getElementById('engine-version') as HTMLSelectElement;
    engineVersionSelect.innerHTML = '';
    const versions = engineVersionInfo.engineVersion[selectedEngine]?.versions || {};

    Object.keys(versions).forEach(version => {
        const option = document.createElement('option');
        option.value = version;
        option.textContent = version;
        engineVersionSelect.appendChild(option);
    });

    const param: ParamsObject = {};
    param["engine"] = selectedEngine;
    UrlParamsManager.setUrlParams(param);

    const firstVersion = Object.keys(versions)[0];
    if (firstVersion) {
        engineVersionSelect.value = firstVersion;
        engineVersionSelect.dispatchEvent(new Event('change'));
        engineDir = `${engineVersionInfo.engineVersion[selectedEngine].savePath}benchmark-${firstVersion}`;
        localStorage.setItem('engineDir', engineDir);
        pageRestart();
    } else {
        console.warn('Unknown engine type:', selectedEngine);
    }
}

function handleEngineVersionChange(event: Event) {
    const target = event.target as HTMLSelectElement;
    let engineDir = "";
    const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
    if (!engineTypeSelect) {
        console.warn('Engine type selector not found');
        return;
    }
    const selectedVersion = target.value;
    if (!selectedVersion) {
        console.warn('No version selected');
        return;
    }
    let param: ParamsObject = {};
    param["version"] = selectedVersion;
    UrlParamsManager.setUrlParams(param);
    if (!event.isTrusted) {
        return;
    }
    const engineConfig = engineVersionInfo.engineVersion[engineTypeSelect.value];
    if (engineConfig) {
        engineDir = `${engineConfig.savePath}benchmark-${selectedVersion}`;
        localStorage.setItem('engineDir', engineDir);
        pageRestart();
    } else {
        console.error('Invalid engine configuration:', engineTypeSelect.value);
    }
}

function handleCanvasSizeChange(event: Event) {
    const target = event.target as HTMLSelectElement;
    const container = document.getElementById('container') as HTMLDivElement;
    const canvas = document.getElementById('benchmark') as HTMLCanvasElement;
    const sidebar = document.getElementById('sidebar') as HTMLDivElement;

    if (target.value === 'full-screen') {
        canvas.style.width = '100%';
        canvas.style.height = '100%';
        container.style.backgroundColor = '';
        canvas.style.border = '';
        canvas.style.boxSizing = '';
        updateSize(shareData);
    } else if (target.value === '2048x1440') {
        container.style.backgroundColor = '#2c2c2c';
        canvas.style.border = '10px solid #ffffff';
        canvas.style.boxSizing = 'content-box';
        canvas.style.backgroundColor='#ffffff';
        const scaleFactor = window.devicePixelRatio;
        canvas.width = 2048;
        canvas.height = 1440;

        const scaleWidth = canvas.width / scaleFactor;
        const scaleHeight = canvas.height / scaleFactor;

        canvas.style.width = scaleWidth + 'px';
        canvas.style.height = scaleHeight + 'px';
        container.style.display = 'flex';
        container.style.justifyContent = 'center';
        container.style.alignItems = 'center';
        shareData.baseView.updateSize(scaleFactor);
    }

    const resolutionSpan = document.querySelector('.resolution');
    resolutionSpan.textContent = `${canvas.width}x${canvas.height}`;

    if (shareData.baseView) {
        shareData.baseView.restartDraw();
    }
}

function showProgress(): void {
    const container = document.getElementById('progressContainer');
    if (container) {
        container.classList.remove('hidden');
    }
    const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
    const versionSelect = document.getElementById('engine-version') as HTMLSelectElement;

    if (engineTypeSelect && versionSelect) {
        const engineType = engineTypeSelect.value;
        const version = versionSelect.value;

        const versionDescSpan = document.getElementById('versionDesc');
        if (versionDescSpan) {
            versionDescSpan.innerHTML = `${engineType}-${version}`;
        }
    }
}

function hideProgress(): void {
    const container = document.getElementById('progressContainer');
    if (container) {
        container.classList.add('fade-out');
        setTimeout(() => {
            container.classList.add('hidden');
            container.classList.remove('fade-out');
            const progressFill = document.getElementById('progressFill');
            if (progressFill) {
                progressFill.style.width = '0%';
            }
        }, 500);
    }
}

function updateProgress(percentage: number): void {
    const progressFill = document.getElementById('progressFill');
    const loadingText = document.getElementById('loadingText');

    if (progressFill) {
        progressFill.style.width = `${percentage}%`;
    }
    if (loadingText) {
        loadingText.textContent = `${percentage}%`;
    }
}

function getRandomInt(min: number, max: number): number {
    return Math.floor(Math.random() * (max - min)) + min;
}

function sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
}


async function loadFontBuffer(fontPath: string): Promise<ArrayBuffer> {
    const response = await fetch(fontPath, {
        headers: {
            'Accept-Encoding': 'gzip, deflate, br'
        },
        cache: 'force-cache'
    });

    if (!response.ok) {
        throw new Error(`load failed: ${fontPath}`);
    }

    return response.arrayBuffer();
}

export async function loadModule(engineDir: string, type: string = "mt") {
    try {
        const baseUrl = getResourceUrl();
        showProgress();
        shareData = new ShareData();
        const Benchmark = await import(`./${engineDir}.js`);
        updateProgress(getRandomInt(10, 30));
        if (type === 'mt') {
            if (!crossOriginIsolated) {
                throw new Error('The current environment does not support multi-threaded WebAssembly, please check the COOP and COEP settings');
            }

            if (typeof SharedArrayBuffer === 'undefined') {
                throw new Error('SharedArrayBuffer is not supported in this environment');
            }
        }
        const moduleConfig = {
            locateFile: (file: string) => {
                const path = `${engineDir}.wasm`;
                console.log('Loading WebAssembly file:', path);
                return path;
            },
            onRuntimeError: (error) => {
                console.error('Runtime error:', error);
            },
            onRuntimeInitialized: () => {
                console.log('WebAssembly runtime initialized successfully');
            },
            onWorkerError: (error) => {
                console.error('Worker error:', error);
            }
        };

        if (type === 'mt') {
            moduleConfig['mainScriptUrlOrBlob'] = `${engineDir}.js`;
        }
        const module = await Benchmark.default(moduleConfig);
        updateProgress(getRandomInt(31, 50));
        if (enumEngineType === EnumEngineType.tgfx) {
            shareData.BenchmarkModule = module;
            TGFXBind(shareData.BenchmarkModule);
            let tgfxView: TGFXBaseView = shareData.BenchmarkModule.TGFXThreadsView.MakeFrom('#benchmark');
            var imagePath = `${baseUrl}/static/image/bridge.jpg`;
            await tgfxView.setImagePath(imagePath);

            var fontPath = `${baseUrl}/static/font/NotoSansSC-Regular.otf`;
            const fontBuffer = await loadFontBuffer(fontPath);
            updateProgress(getRandomInt(51, 70));
            const fontUIntArray = new Uint8Array(fontBuffer);

            var emojiFontPath = `${baseUrl}/static/font/NotoColorEmoji.ttf`;
            const emojiFontBuffer = await loadFontBuffer(emojiFontPath);
            updateProgress(getRandomInt(71, 90));
            const emojiFontUIntArray = new Uint8Array(emojiFontBuffer);


            tgfxView.registerFonts(fontUIntArray, emojiFontUIntArray);
            shareData.baseView = tgfxView;
        } else if (enumEngineType === EnumEngineType.skia) {
            let skiaView: SkiaView = module.SkiaView.MakeFrom('#benchmark');

            var fontPath = `${baseUrl}/static/font/SFNSRounded.ttf`;
            const fontBuffer = await loadFontBuffer(fontPath);
            updateProgress(getRandomInt(51, 70));
            const fontUIntArray = new Uint8Array(fontBuffer);

            var emojiFontPath = `${baseUrl}/static/font/NotoColorEmoji.ttf`;
            const emojiFontBuffer = await loadFontBuffer(emojiFontPath);
            updateProgress(getRandomInt(71, 90));
            const emojiFontUIntArray = new Uint8Array(emojiFontBuffer);

            skiaView.registerFonts(fontUIntArray, emojiFontUIntArray);
            shareData.baseView = skiaView;
        }
        shareData.baseView.showPerfData(false);
        updateProgress(100);
        hideProgress();
        updateSize(shareData);
        startDraw(shareData);
        setDrawParamFromUrl();
    } catch (error) {
        console.error(error);
        throw new Error(`Failed to load ${type} version. Please check the wasm file path!`);
    }
}

async function handleThreadTypeChange(event: Event) {
    const target = event.target as HTMLInputElement;
    if (target.type === 'radio' && target.name === 'thread-type' && target.checked) {
        const threadType = target.value;
        localStorage.setItem('threadType', threadType);
        console.log(`Thread type changed to ${threadType}`);
        await loadModule(threadType);
        pageRestart();
    }
}

export function bindEventListeners() {

    const threadTypeRadioGroup = document.querySelector('.radio-group-thread-type');
    if (threadTypeRadioGroup) {
        threadTypeRadioGroup.addEventListener('change', handleThreadTypeChange);
    }

    const restartBtn = document.getElementById('restartBtn');
    if (restartBtn) {
        restartBtn.addEventListener('click', () => {
            if (!shareData.baseView) {
                return;
            }
            shareData.baseView.restartDraw();
        });
    }

    const configParamContainer = document.getElementById('config-param');
    if (configParamContainer) {
        configParamContainer.addEventListener('change', handleConfigParamChange);
    }

    const graphicTypeRadioGroup = document.getElementById('radio-group-graphic-type');
    if (graphicTypeRadioGroup) {
        graphicTypeRadioGroup.addEventListener('change', handleGraphicTypeChange);
    }

    const engineTypeSelect = document.getElementById('engine-type-select');
    engineTypeSelect.addEventListener('change', handleEngineTypeChange);

    const engineVersionSelect = document.getElementById('engine-version');
    engineVersionSelect.addEventListener('change', handleEngineVersionChange);

    const canvasSizeSelect = document.getElementById('canvas-size-select');
    if (canvasSizeSelect) {
        canvasSizeSelect.addEventListener('change', handleCanvasSizeChange);
    }

}

function urlParamConvertHtmlParam(urlParam: string): string {
    urlParam = urlParam.toString().toLowerCase();
    if (urlParam === "start") {
        return "startCount";
    } else if (urlParam === "step") {
        return "stepCount";
    } else if (urlParam === "max") {
        return "maxShapes";
    } else if (urlParam === "min") {
        return "minFPS";
    } else if (urlParam === 'graphic') {
        return "graphicType";
    } else if (urlParam === 'engine') {
        return "engineType";
    } else if (urlParam === 'version') {
        return "engineVersion";
    }
}

function htmlParamConvertUrlParam(htmlParam: string): string {
    htmlParam = htmlParam.toString().toLowerCase();
    if (htmlParam === "startcount") {
        return "start";
    } else if (htmlParam === "stepcount") {
        return "step";
    } else if (htmlParam === "maxshapes") {
        return "max";
    } else if (htmlParam === "minfps") {
        return "min";
    } else if (htmlParam === 'graphictype') {
        return "graphic";
    } else if (htmlParam === 'enginetype') {
        return "engine";
    } else if (htmlParam === 'engineversion') {
        return "version";
    }
    return "";
}

export class UrlParamsManager {
    static setUrlParams(params: ParamsObject): void {
        const url = new URL(window.location.href);
        const searchParams = new URLSearchParams(url.search);
        for (const [key, value] of Object.entries(params)) {
            searchParams.set(key, value.toString().toLowerCase());
        }
        const newUrl = `${window.location.pathname}?${searchParams.toString()}`;
        window.history.pushState({}, '', newUrl);
    }

    static getUrlParams(): ParamsObject {
        const searchParams = new URLSearchParams(window.location.search);
        const params: ParamsObject = {};
        searchParams.forEach((value, key) => {
            key = key.toString().toLowerCase();
            if (!isNaN(Number(value))) {
                params[key] = Number(value);
            } else {
                params[key] = value;
            }
        });

        return params;
    }

    static setUrlParamsAndReload(params: ParamsObject): void {
        const url = new URL(window.location.href);
        const searchParams = new URLSearchParams(url.search);

        for (const [key, value] of Object.entries(params)) {
            searchParams.set(key, value.toString());
        }
        window.location.search = searchParams.toString();
    }


    static removeUrlParam(paramKey: string): void {
        const url = new URL(window.location.href);
        const searchParams = new URLSearchParams(url.search);

        Array.from(searchParams.keys()).forEach(key => {
            if (key.toLowerCase() === paramKey.toLowerCase()) {
                searchParams.delete(key);
            }
        });

        const newUrl = `${window.location.pathname}?${searchParams.toString()}`;
        window.history.pushState({}, '', newUrl);
    }

    static validateUrlParams(params: ParamsObject): boolean {
        if (params['engine'] !== undefined) {
            const engineValue = params['engine'].toString().toLowerCase();
            if (!['tgfx', 'skia'].includes(engineValue)) {
                return false;
            }
            if (engineValue === defaultUrlParams['engine']) {
                UrlParamsManager.removeUrlParam('engine');
            }
        }

        if (params['engine'] !== undefined && params['version'] !== undefined) {
            const engineType = params['engine'].toString().toLowerCase();
            const version = params['version'].toString();
            if (!engineVersionInfo?.engineVersion?.[engineType]) {
                return false;
            }
            const versions = engineVersionInfo.engineVersion[engineType].versions;
            const hasMatchingVersion = Object.keys(versions).some(v =>
                v.toLowerCase() === version.toLowerCase()
            );
            if (!hasMatchingVersion) {
                return false;
            }
        } else if (params['version'] !== undefined) {
            const version = params['version'].toString();
            const versions = engineVersionInfo.engineVersion[defaultUrlParams['engine']].versions;
            const hasMatchingVersion = Object.keys(versions).some(v =>
                v.toLowerCase() === version.toLowerCase()
            );
            if (!hasMatchingVersion) {
                return false;
            }
        }

        if (params['start'] !== undefined) {
            const value = Number(params['start']);
            if (isNaN(value) || value <= 0) {
                return false;
            }
            if (value === Number(defaultUrlParams['start'])) {
                UrlParamsManager.removeUrlParam('start');
            }
        }


        if (params['step'] !== undefined) {
            const value = Number(params['step']);
            if (isNaN(value) || value <= 0) {
                return false;
            }
            if (value === Number(defaultUrlParams['step'])) {
                UrlParamsManager.removeUrlParam('step');
            }
        }

        if (params['max'] !== undefined) {
            const value = Number(params['max']);
            if (isNaN(value) || value <= 0) {
                return false;
            }
            if (value === Number(defaultUrlParams['max'])) {
                UrlParamsManager.removeUrlParam('max');
            }
        }

        if (params['min'] !== undefined) {
            const value = Number(params['min']);
            if (isNaN(value) || value <= 0) {
                return false;
            }
            if (value === Number(defaultUrlParams['min'])) {
                UrlParamsManager.removeUrlParam('min');
            }
        }

        if (params['graphic'] !== undefined) {
            const graphicValue = params['graphic'].toString().toLowerCase();
            const graphicTypes = graphicTypeStr.map(type => type.toLowerCase());

            if (!graphicTypes.includes(graphicValue)) {
                return false;
            }
            if (graphicValue === defaultUrlParams['graphic'].toString().toLowerCase()) {
                UrlParamsManager.removeUrlParam('graphic');
            }
        }

        const currentParams = new URLSearchParams(window.location.search);
        if (currentParams.toString() === '') {
            const url = new URL(window.location.href);
            url.search = '';
            window.history.replaceState({}, '', url.toString());
        }
        return true;
    }

    static resetUrlToDefault(): void {
        const url = new URL(window.location.href);
        url.search = '';
        window.history.replaceState({}, '', url.toString());
        UrlParamsManager.setUrlParams(defaultUrlParams);
    }

    static clearUrlParams(): void {
        const url = new URL(window.location.href);
        url.search = '';
        window.history.replaceState({}, '', url.toString());
    }
}

export function setDrawParamFromUrl() {
    let param: ParamsObject = UrlParamsManager.getUrlParams();
    if (param["start"]) {
        drawParam.startCount = Number(param["start"]);
    }
    if (param["step"]) {
        drawParam.stepCount = Number(param["step"]);
    }
    if (param["max"]) {
        drawParam.maxCount = Number(param["max"]);
    }
    if (param["min"]) {
        drawParam.minFPS = Number(param["min"]);
    }
    shareData.baseView.updateDrawParam(drawParam);
    if (param["graphic"]) {
        setGraphicType(param["graphic"].toString());
    }
}

function webUpdateGraphicType(type:number) {
    try {
        if (typeof type !== 'number' || type < 0 || type >= graphicTypeStr.length) {
            console.warn('Invalid graphic type: ' + type);
            return;
        }

        const typeStr = graphicTypeStr[Number(type)];
        if (!typeStr) {
            console.warn('No string mapping for graphic type: ' + type);
            return;
        }

        const radioGroup = document.getElementById('radio-group-graphic-type');
        if (!radioGroup) {
            console.error('Radio group not found');
            return;
        }
        const targetRadio = radioGroup.querySelector('input[value="' + typeStr + '"]');
        if (!(targetRadio instanceof HTMLInputElement)) {
            console.error('Radio button for ' + typeStr + ' not found');
            return;
        }
        targetRadio.checked = true;
        console.log('Updated graphic type to: ' + typeStr);
        const event = new Event('change', { bubbles: true });
        targetRadio.dispatchEvent(event);
    } catch (error) {
        console.error('Error updating graphic type:', error);
    }
}

export function setupCoordinateConversion(canvasId: string, showSideBar: boolean) {
    console.log("setupCoordinateConversion");
    const canvas = document.getElementById(canvasId) as HTMLCanvasElement;

    let isProcessingMouseEvent = false;
    let lastClickTime = 0;
    const CLICK_DEBOUNCE_TIME = 100;
    canvas.addEventListener('click', (e: MouseEvent) => {
        const now = Date.now();
        if (now - lastClickTime < CLICK_DEBOUNCE_TIME) {
            e.stopImmediatePropagation();
            e.preventDefault();
            return;
        }
        lastClickTime = now;
        if (isProcessingMouseEvent) {
            e.stopImmediatePropagation();
            e.preventDefault();
            return;
        }

        isProcessingMouseEvent = true;
        try {
            const convertedEvent = convertCoordinates(e, showSideBar);

            Object.defineProperty(e, 'clientX', {
                value: convertedEvent.clientX,
                writable: true
            });
            Object.defineProperty(e, 'clientY', {
                value: convertedEvent.clientY,
                writable: true
            });
            drawIndex++;
            webUpdateGraphicType(drawIndex % graphicTypeStr.length);
        } finally {
            isProcessingMouseEvent = false;
        }
    }, {capture: true});

    let isProcessingMouseMove = false;
    canvas.addEventListener('mousemove', (e: MouseEvent) => {
        if (isProcessingMouseMove) {
            return;
        }
        isProcessingMouseMove = true;
        try {
            const convertedEvent = convertCoordinates(e, showSideBar);
            Object.defineProperty(e, 'clientX', {
                value: convertedEvent.clientX,
                writable: true
            });
            Object.defineProperty(e, 'clientY', {
                value: convertedEvent.clientY,
                writable: true
            });

        } finally {
            isProcessingMouseMove = false;
        }
    }, {capture: true});
}


function convertCoordinates(e: MouseEvent, showSideBar: boolean) {
    let sidebarWidth = 0;
    let offsetX = 0;
    let offsetY = 0;

    if (showSideBar) {
        const sidebar = document.getElementById('sidebar');
        sidebarWidth = sidebar ? sidebar.clientWidth : 0;
        offsetX = sidebarWidth;
    }

    const benchmark = document.getElementById('benchmark');
    const container = document.getElementById('container');

    if (benchmark && container) {
        const benchmarkWidth = benchmark.clientWidth;
        const benchmarkHeight = benchmark.clientHeight;
        const containerWidth = container.clientWidth;
        const containerHeight = container.clientHeight;

        if (benchmarkWidth !== containerWidth && benchmarkHeight !== containerHeight) {
            offsetX = (containerWidth - benchmarkWidth) / 2 + sidebarWidth;
            offsetY = (containerHeight - benchmarkHeight) / 2;
        }
    }

    return {
        clientX: (e.clientX - offsetX),
        clientY: (e.clientY - offsetY)
    };
}