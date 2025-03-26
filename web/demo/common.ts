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

class BaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public restartDraw: () => void;
    public updateDrawParam: (type: number, value: number) => void;
    public updateGraphicType: (type: number) => void;
}

export class TGFXBaseView extends BaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public restartDraw: () => void;
    public updateDrawParam: (type: number, value: number) => void;
    public updateGraphicType: (type: number) => void;
}

export class SkiaView extends BaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public restartDraw: () => void;
    public updateDrawParam: (type: number, value: number) => void;
    public updateGraphicType: (type: number) => void;
}

export class ShareData {
    public BenchmarkModule: types.TGFX = null;
    public baseView: BaseView = null;
    public resized: boolean = false;
}

export let shareData: ShareData;

enum EnumEngineType {
    tgfx = "tgfx",
    skia = "skia"
}

export let enumEngineType: EnumEngineType;

export enum DataType {
    startCount = 0,
    stepCount = 1,
    maxDrawCount = 2,
    minFPS = 3,
}

export enum GraphicType {
    Rect = 0,
    Circle = 1,
    Oval = 2,
    RRect = 3
}

const graphicTypeStr: readonly ['Rect', 'Circle', 'Oval', 'RRect'] = ['Rect', 'Circle', 'Oval', 'RRect'];

export let engineVersionInfo: SideBarConfig;

export interface ParamsObject {
    [key: string]: string | number | boolean;
}

export let defaultUrlParams: ParamsObject = {
    engine: "tgfx",
    version: '',
    start: 1,
    step: 600,
    max: 1000000,
    min: 60,
    graphic: "Rect",
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

export function parseSideBarParam(): SideBarConfig {
    try {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', 'static/sideBarConfigParam.json', false);
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
    let queryString = window.location.search;
    if (queryString !== '') {
        const urlParams = UrlParamsManager.getUrlParams();
        if (!UrlParamsManager.validateUrlParams(urlParams)) {
            queryString = '';
            UrlParamsManager.clearUrlParams();
        }
    }

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

    if (queryString === '') {
        param['engine'] = engineTypeSelect.value;
        param['version'] = versionSelect.value;

        const configInputs = document.querySelectorAll('#config-param input[type="number"]');
        configInputs.forEach(input => {
            const element = input as HTMLInputElement;
            if (element.value) {
                let key: string = htmlParamConvertUrlParam(element.id);
                param[key] = Number(element.value);
            }
        });
        const selectedInput = document.querySelector('input[name="graphic-type"]:checked') as HTMLInputElement;
        if (selectedInput) {
            param["graphic"] = selectedInput.value;
        }
        UrlParamsManager.setUrlParams(param);
    } else {
        try {
            param = UrlParamsManager.getUrlParams();
            if (param['engine']) {
                engineTypeSelect.value = param['engine'].toString().toLowerCase();
            }
            if (param['version']) {
                versionSelect.value = param['version'].toString();
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
        stepCount: 600,
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
    let type: DataType = DataType.startCount;
    if (target.id === 'startCount') {
        type = DataType.startCount;
    } else if (target.id === 'stepCount') {
        type = DataType.stepCount;
    } else if (target.id === 'maxShapes') {
        type = DataType.maxDrawCount;
    } else if (target.id === 'minFPS') {
        type = DataType.minFPS;
    } else {
        console.error('Unknown parameter type:', target.id);
    }
    shareData.baseView.updateDrawParam(type, Number(target.value));
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
        default:
            type = GraphicType.Rect;
    }
    if (shareData.baseView) {
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

    const firstVersion = Object.keys(versions)[0];
    if (firstVersion) {
        engineVersionSelect.value = firstVersion;
        engineDir = `${engineVersionInfo.engineVersion[selectedEngine].savePath}benchmark-${firstVersion}`;
        localStorage.setItem('engineDir', engineDir);
        const url = window.location.href.split('?')[0];
        window.history.replaceState({}, '', url);
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
    const engineConfig = engineVersionInfo.engineVersion[engineTypeSelect.value];
    if (engineConfig) {
        engineDir = `${engineConfig.savePath}benchmark-${selectedVersion}`;
        localStorage.setItem('engineDir', engineDir);
        pageRestart();
    } else {
        console.error('Invalid engine configuration:', engineTypeSelect.value);
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

export async function loadModule(engineDir: string, type: string = "mt") {
    try {
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
            console.log(`BenchmarkModule:${shareData.BenchmarkModule}`);
            TGFXBind(shareData.BenchmarkModule);
            let tgfxView: TGFXBaseView = shareData.BenchmarkModule.TGFXThreadsView.MakeFrom('#benchmark');
            var imagePath = "static/image/bridge.jpg";
            await tgfxView.setImagePath(imagePath);

            var fontPath = "static/font/NotoSansSC-Regular.otf";
            const fontBuffer = await fetch(fontPath).then((response) => response.arrayBuffer());
            updateProgress(getRandomInt(51, 70));
            const fontUIntArray = new Uint8Array(fontBuffer);
            var emojiFontPath = "static/font/NotoColorEmoji.ttf";
            const emojiFontBuffer = await fetch(emojiFontPath).then((response) => response.arrayBuffer());
            updateProgress(getRandomInt(71, 90));
            const emojiFontUIntArray = new Uint8Array(emojiFontBuffer);
            tgfxView.registerFonts(fontUIntArray, emojiFontUIntArray);
            shareData.baseView = tgfxView;
        } else if (enumEngineType === EnumEngineType.skia) {
            let skiaView: SkiaView = module.SkiaView.MakeFrom('#benchmark');
            var fontPath = "static/font//SFNSRounded.ttf";
            const fontBuffer = await fetch(fontPath).then((response) => response.arrayBuffer());
            updateProgress(getRandomInt(51, 70));
            const fontUIntArray = new Uint8Array(fontBuffer);
            var emojiFontPath = "static/font/NotoColorEmoji.ttf";
            const emojiFontBuffer = await fetch(emojiFontPath).then((response) => response.arrayBuffer());
            updateProgress(getRandomInt(71, 90));
            const emojiFontUIntArray = new Uint8Array(emojiFontBuffer);
            skiaView.registerFonts(fontUIntArray, emojiFontUIntArray);
            shareData.baseView = skiaView;
        }
        updateSize(shareData);
        updateProgress(100);
        hideProgress();
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
            searchParams.set(key, value.toString());
        }
        const newUrl = `${window.location.pathname}?${searchParams.toString()}`;
        window.history.pushState({}, '', newUrl);
    }

    static getUrlParams(): ParamsObject {
        const searchParams = new URLSearchParams(window.location.search);
        const params: ParamsObject = {};
        searchParams.forEach((value, key) => {
            key = key.toString().toLowerCase();
            if (value === 'true') {
                params[key] = true;
            } else if (value === 'false') {
                params[key] = false;
            } else if (!isNaN(Number(value))) {
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

        searchParams.delete(paramKey);

        const newUrl = `${window.location.pathname}?${searchParams.toString()}`;
        window.history.pushState({}, '', newUrl);
    }

    static validateUrlParams(params: ParamsObject): boolean {
        for (const key of Object.keys(defaultUrlParams)) {
            if (!(key in params)) {
                return false;
            }
        }
        if (params['engine'] !== undefined && !['tgfx', 'skia'].includes(params['engine'].toString().toLowerCase())) {
            return false;
        }

        if (params['engine'] !== undefined && params['version'] !== undefined) {
            const engineType = params['engine'].toString().toLowerCase();
            const version = params['version'].toString();
            if (!engineVersionInfo?.engineVersion?.[engineType]) {
                return false;
            }
            if (!engineVersionInfo.engineVersion[engineType].versions?.[version]) {
                return false;
            }
        }
        if ((isNaN(Number(params['start'])) || Number(params['start']) <= 0)) {
            return false;
        }
        if ((isNaN(Number(params['step'])) || Number(params['step']) <= 99)) {
            return false;
        }
        if ((isNaN(Number(params['max'])) || Number(params['max']) <= 9999)) {
            return false;
        }
        if ((isNaN(Number(params['min'])) || Number(params['min']) <= 19)) {
            return false;
        }
        const graphicTypes = graphicTypeStr.map(type => type.toLowerCase());
        if (params['graphic'] !== undefined && !graphicTypes.includes(params['graphic'].toString().toLowerCase())) {
            return false;
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
        shareData.baseView.updateDrawParam(DataType.startCount, Number(param["start"]));
    }
    if (param["step"]) {
        shareData.baseView.updateDrawParam(DataType.stepCount, Number(param["step"]));
    }
    if (param["max"]) {
        shareData.baseView.updateDrawParam(DataType.maxDrawCount, Number(param["max"]));
    }
    if (param["min"]) {
        shareData.baseView.updateDrawParam(DataType.minFPS, Number(param["min"]));
    }
    if (param["graphic"]) {
        setGraphicType(param["graphic"].toString());
    }
}