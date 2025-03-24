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
};

export enum GraphicType {
    rectangle = 0,
    round = 1,
    roundedRectangle = 2,
    oval = 3,
    simpleGraphicBlending = 4,
    complexGraphics = 5
};

export let engineVensionInfo;

export interface ParamsObject {
    [key: string]: string | number | boolean;
}

export let defaultParams: ParamsObject = {
    engineType: "tgfx",
    engineVersion: '',
    startCount: 1,
    stepCount: 600,
    MaxDrawCount: 1000000,
    minFps: 60,
    graphicType: "rectangle",
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
            throw new Error('配置文件格式错误：缺少 engineVersion 配置项');
        }

        for (const engine of ['tgfx', 'skia'] as const) {
            const engineConfig = config.engineVersion[engine];
            if (engineConfig) {
                if (!engineConfig.savePath) {
                    throw new Error(`配置文件格式错误：${engine.toUpperCase()} 引擎缺少 savePath`);
                }
                if (!engineConfig.versions || Object.keys(engineConfig.versions).length === 0) {
                    throw new Error(`配置文件格式错误：${engine.toUpperCase()} 引擎缺少版本信息`);
                }

                for (const [version, threadTypes] of Object.entries(engineConfig.versions)) {
                    if (!Array.isArray(threadTypes) || threadTypes.length === 0) {
                        throw new Error(`配置文件格式错误：${engine.toUpperCase()} 引擎 ${version} 版本的线程类型配置无效`);
                    }
                    if (!threadTypes.every(type => type === 'st' || type === 'mt')) {
                        throw new Error(`配置文件格式错误：${engine.toUpperCase()} 引擎 ${version} 版本包含无效的线程类型`);
                    }
                }
            }
        }

        if (!config.engineVersion.tgfx && !config.engineVersion.skia) {
            throw new Error('配置文件格式错误：至少需要一个引擎配置');
        }
        engineVensionInfo = config;
        return config;
    } catch (error) {
        console.error('解析侧边栏配置文件失败:', error);
        throw error;
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
    const queryString = window.location.search;

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

    function updateThreadTypeOptions(engineType: string, version: string) {
        const stRadioItem = document.getElementById('st-radio-item');
        const mtRadioItem = document.getElementById('mt-radio-item');

        const engineConfig = config.engineVersion[engineType as keyof typeof config.engineVersion];
        if (!engineConfig || !engineConfig.versions[version]) return;

        const threadTypes = engineConfig.versions[version];

        if (stRadioItem) {
            stRadioItem.style.display = threadTypes.includes('st') ? '' : 'none';
        }

        if (mtRadioItem) {
            mtRadioItem.style.display = threadTypes.includes('mt') ? '' : 'none';
        }

        if (threadTypes.length === 1) {
            const radio = document.querySelector(`input[name="thread-type"][value="${threadTypes[0]}"]`) as HTMLInputElement;
            if (radio) {
                radio.checked = true;
            }
        }
    }

    handleEngineVisibility();

    const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
    if (engineTypeSelect) {
        updateVersionSelect(engineTypeSelect.value);
        if (versionSelect) {
            updateThreadTypeOptions(engineTypeSelect.value, versionSelect.value);
        }
    }

    if (engineTypeSelect) {
        engineTypeSelect.addEventListener('change', (e) => {
            const target = e.target as HTMLSelectElement;
            updateVersionSelect(target.value);
            if (versionSelect) {
                updateThreadTypeOptions(target.value, versionSelect.value);
            }
        });
    }

    if (versionSelect) {
        versionSelect.addEventListener('change', (e) => {
            const target = e.target as HTMLSelectElement;
            const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
            if (engineTypeSelect) {
                updateThreadTypeOptions(engineTypeSelect.value, target.value);
            }
        }, true);
    }

    if (localStorage.getItem('needRestore') === 'true') {
        restorePageSettings();
    }

    if (queryString === '') {
        param['engineType'] = engineTypeSelect.value;
        param['engineVersion'] = versionSelect.value;

        const configInputs = document.querySelectorAll('#config-param input[type="number"]');
        configInputs.forEach(input => {
            const element = input as HTMLInputElement;
            if (element.value) {
                param[element.id] = Number(element.value);
            }
        });
        const selectedInput = document.querySelector('input[name="graphic-type"]:checked') as HTMLInputElement;
        if (selectedInput) {
            param["graphicType"] = selectedInput.value;
        }
        UrlParamsManager.setUrlParams(param);
    } else {
        try {
            param = UrlParamsManager.getUrlParams();
            if (param['engineType']) {
                engineTypeSelect.value = param['engineType'].toString().toLowerCase();
            }
            if (param['engineVersion']) {
                versionSelect.value = param['engineVersion'].toString();
            }
            const configInputs = document.querySelectorAll('#config-param input[type="number"]');

            configInputs.forEach(input => {
                const element = input as HTMLInputElement;
                if (element.id in param && param[element.id] !== undefined) {
                    const value = Number(param[element.id]);
                    if (typeof value === 'number') {
                        element.value = value.toString();
                    } else {
                        console.warn(`参数 ${element.id} 的值不是数字类型`);
                    }
                }
            });

            if (param['graphicType']) {
                let graphicType = param['graphicType'].toString().toLowerCase();
                // const selectedInput = document.querySelector(`input[name="graphic-type"][value="${graphicType}"]`) as HTMLInputElement;
                const selectedInput = Array.from<HTMLInputElement>(document.querySelectorAll('input[name="graphic-type"]'))
                    .find(input => input.value.toLowerCase() === graphicType.toLowerCase()) as HTMLInputElement;

                if (selectedInput) {
                    selectedInput.checked = true;
                }
            }

        } catch (error) {
            console.error('url参数错误:', error);
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
    threadType: {
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

    const threadTypeInputs = document.getElementsByName('thread-type') as NodeListOf<HTMLInputElement>;
    const threadType = {
        options: Array.from(threadTypeInputs).map(input => input.value),
        selected: Array.from(threadTypeInputs).find(input => input.checked)?.value || ''
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
        threadType,
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
        console.error('解析保存的设置失败:', error);
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
        console.error('解析保存的设置失败:', error);
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
                param["engineVersion"] = settings.engineVersion.selected;
            }
        }

        settings.threadType.options.forEach(threadType => {
            const radioItem = document.getElementById(`${threadType}-radio-item`);
            if (radioItem) {
                radioItem.style.display = '';

                const radio = radioItem.querySelector(`input[value="${threadType}"]`) as HTMLInputElement;
                if (radio) {
                    if (threadType === settings.threadType.selected) {
                        radio.checked = true;
                    }
                }
            }
        });

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
        console.error('恢复页面设置失败:', error);
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
    const rectangleRadio = document.querySelector('input[name="graphic-type"][value="rectangle"]') as HTMLInputElement;
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
            return `${engineVensionInfo.engineVersion.tgfx.savePath}benchmark-${version}`;
        } else if (engineTypeSelect.value === 'skia') {
            return `${engineVensionInfo.engineVersion.skia.savePath}benchmark-${version}`;
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
    param[target.id] = Number(target.value);
    if (target.id === 'startCount') {
        shareData.baseView.updateDrawParam(DataType.startCount, Number(target.value));
    } else if (target.id === 'stepCount') {
        shareData.baseView.updateDrawParam(DataType.stepCount, Number(target.value));
    } else if (target.id === 'maxShapes') {
        shareData.baseView.updateDrawParam(DataType.maxDrawCount, Number(target.value));
    } else if (target.id === 'minFPS') {
        shareData.baseView.updateDrawParam(DataType.minFPS, Number(target.value));
    }
    UrlParamsManager.setUrlParams(param);
}

function setGraphicType(graphicType: string) {
    graphicType=graphicType.toLowerCase();
    let type: GraphicType;
    switch (graphicType) {
        case 'rectangle':
            type = GraphicType.rectangle;
            break;
        case 'round':
            type = GraphicType.round;
            break;
        case 'roundedRectangle'.toLowerCase():
            type = GraphicType.roundedRectangle;
            break;
        case 'oval':
            type = GraphicType.oval;
            break;
        case 'simpleGraphicBlending'.toLowerCase():
            type = GraphicType.simpleGraphicBlending;
            break;
        case 'complexGraphics'.toLowerCase():
            type = GraphicType.complexGraphics;
            break;
        default:
            type = GraphicType.rectangle;
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
        setGraphicType(graphicType);
        param["graphicType"] = graphicType;
        UrlParamsManager.setUrlParams(param);
    }
}

function handleEngineTypeChange(event: Event) {
    const target = event.target as HTMLSelectElement;
    let engineDir = "";
    const selectedEngine = target.value;
    const engineVersionSelect = document.getElementById('engine-version') as HTMLSelectElement;
    engineVersionSelect.innerHTML = '';
    const versions = engineVensionInfo.engineVersion[selectedEngine]?.versions || {};
    // let param: ParamsObject = {};
    // param["engineType"] = selectedEngine.toLowerCase();
    // UrlParamsManager.setUrlParams(param);

    Object.keys(versions).forEach(version => {
        const option = document.createElement('option');
        option.value = version;
        option.textContent = version;
        engineVersionSelect.appendChild(option);
    });

    const firstVersion = Object.keys(versions)[0];
    if (firstVersion) {
        engineVersionSelect.value = firstVersion;
        engineDir = `${engineVensionInfo.engineVersion[selectedEngine].savePath}benchmark-${firstVersion}`;
        localStorage.setItem('engineDir', engineDir);
        const url = window.location.href.split('?')[0]; // 去掉查询字符串部分
        window.history.replaceState({}, '', url);
        pageRestart();
    } else {
        console.warn('未知的引擎类型:', selectedEngine);
    }
}

function handleEngineVersionChange(event: Event) {
    const target = event.target as HTMLSelectElement;
    let engineDir = "";
    const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
    if (!engineTypeSelect) {
        console.warn('未找到引擎类型选择器');
        return;
    }
    const selectedVersion = target.value;
    if (!selectedVersion) {
        console.warn('未选择版本');
        return;
    }
    let param: ParamsObject = {};
    param["engineVersion"] = selectedVersion;
    UrlParamsManager.setUrlParams(param);
    const engineConfig = engineVensionInfo.engineVersion[engineTypeSelect.value];
    if (engineConfig) {
        engineDir = `${engineConfig.savePath}benchmark-${selectedVersion}`;
        localStorage.setItem('engineDir', engineDir);
        pageRestart();
    } else {
        console.error('无效的引擎配置:', engineTypeSelect.value);
    }
}

export async function loadModule(engineDir: string, type: string = "mt") {
    try {
        shareData = new ShareData();
        const Benchmark = await import(`./${engineDir}.js`);
        console.log('load BenchmarkModule');
        if (type === 'mt') {
            if (!crossOriginIsolated) {
                console.error('当前环境不支持多线程WebAssembly，请检查COOP和COEP设置');
                throw new Error('多线程WebAssembly不可用');
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
        if (enumEngineType === EnumEngineType.tgfx) {
            shareData.BenchmarkModule = module;
            console.log(`BenchmarkModule:${shareData.BenchmarkModule}`);
            TGFXBind(shareData.BenchmarkModule);
            let tgfxView: TGFXBaseView = shareData.BenchmarkModule.TGFXThreadsView.MakeFrom('#benchmark');

            var imagePath = "static/image/bridge.jpg";
            await tgfxView.setImagePath(imagePath);

            var fontPath = "static/font/NotoSansSC-Regular.otf";
            const fontBuffer = await fetch(fontPath).then((response) => response.arrayBuffer());
            const fontUIntArray = new Uint8Array(fontBuffer);
            var emojiFontPath = "static/font/NotoColorEmoji.ttf";
            const emojiFontBuffer = await fetch(emojiFontPath).then((response) => response.arrayBuffer());
            const emojiFontUIntArray = new Uint8Array(emojiFontBuffer);
            tgfxView.registerFonts(fontUIntArray, emojiFontUIntArray);
            shareData.baseView = tgfxView;
        } else if (enumEngineType === EnumEngineType.skia) {
            let skiaView: SkiaView = module.SkiaView.MakeFrom('#benchmark');
            var fontPath = "static/font//SFNSRounded.ttf";
            const fontBuffer = await fetch(fontPath).then((response) => response.arrayBuffer());
            const fontUIntArray = new Uint8Array(fontBuffer);
            var emojiFontPath = "static/font/NotoColorEmoji.ttf";
            const emojiFontBuffer = await fetch(emojiFontPath).then((response) => response.arrayBuffer());
            const emojiFontUIntArray = new Uint8Array(emojiFontBuffer);
            skiaView.registerFonts(fontUIntArray, emojiFontUIntArray);
            shareData.baseView = skiaView;
        }

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
}

export function setDrawParamFromUrl() {
    let param: ParamsObject = UrlParamsManager.getUrlParams();
    if (param["startCount"]) {
        shareData.baseView.updateDrawParam(DataType.startCount, Number(param["startCount"]));
    } else if (param["stepCount"]) {
        shareData.baseView.updateDrawParam(DataType.stepCount, Number(param["stepCount"]));
    } else if (param["maxShapes"]) {
        shareData.baseView.updateDrawParam(DataType.maxDrawCount, Number(param["maxShapes"]));
    } else if (param["minFPS"]) {
        shareData.baseView.updateDrawParam(DataType.minFPS, Number(param["minFPS"]));
    }
    if (param["graphicType"]) {
        setGraphicType(param["graphicType"].toString().toLowerCase());
    }
}