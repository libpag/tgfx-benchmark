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
    public setAntiAlias: (antiAlia: boolean) => void;
    public setStroke: (stroke: boolean) => void;
}

export class TGFXBaseView extends BaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public restartDraw: () => void;
    public updateDrawParam: (drawParam: DrawParam) => void;
    public updateGraphicType: (type: number) => void;
    public showPerfData: (status: boolean) => void;
    public init: () => void;
    public setAntiAlias: (antiAlia: boolean) => void;
    public setStroke: (stroke: boolean) => void;
}

export class SkiaView extends BaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public restartDraw: () => void;
    public updateDrawParam: (drawParam: DrawParam) => void;
    public updateGraphicType: (type: number) => void;
    public showPerfData: (status: boolean) => void;
    public init: () => void;
    public setAntiAlias: (antiAlia: boolean) => void;
    public setStroke: (stroke: boolean) => void;
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
const canvasSizeOption = []

export enum GraphicType {
    Rect = 0,
    Circle = 1,
    Oval = 2,
    RRect = 3,
    Star = 4
}


const graphicTypeStr: readonly ['Rect', 'Circle', 'Oval', 'RRect'] = ['Rect', 'Circle', 'Oval', 'RRect'];

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
    const canvasSizeSelect = document.getElementById('canvas-size-select') as HTMLSelectElement;
    let scaleFactor = window.devicePixelRatio;
    if (canvasSizeSelect.value !== '2048x1440') {
        let canvas = document.getElementById('benchmark') as HTMLCanvasElement;
        let container = document.getElementById('container') as HTMLDivElement;
        let screenRect = container.getBoundingClientRect();
        canvas.width = screenRect.width * scaleFactor;
        canvas.height = screenRect.height * scaleFactor;
        canvas.style.width = screenRect.width + "px";
        canvas.style.height = screenRect.height + "px";

        const resolutionSpan = document.querySelector('.resolution');
        resolutionSpan.textContent = `${canvas.width} x ${canvas.height}`;
        canvasSizeSelect.value = 'full-screen';
        localStorage.setItem('canvasSize', 'full-screen');
    }
    shareData.baseView.updateSize(scaleFactor);
}

export function startDraw(shareData: ShareData) {
    if (!shareData.baseView) {
        return;
    }
    shareData.baseView.startDraw();
}

export function onresizeEvent(shareData: ShareData) {
    if (!shareData || !shareData.baseView) {
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


function validateObjectStructure(obj: any, template: any, path: string = 'root'): boolean {
    for (const key in obj) {
        if (key === '' || key.trim() === '') {
            console.warn(`Empty key found at ${path}`);
            return false;
        }

        if (!(key in template)) {
            console.warn(`Unexpected field: ${path}.${key}`);
            return false;
        }
    }

    for (const key in template) {
        if (!(key in obj)) {
            console.warn(`Missing field: ${path}.${key}`);
            return false;
        }

        const templateValue = template[key];
        const objValue = obj[key];

        if (objValue === null || objValue === undefined) {
            console.warn(`Empty value at ${path}.${key}: value is ${objValue}`);
            return false;
        }

        if (typeof objValue === 'string' && objValue.trim() === '') {
            console.warn(`Empty string at ${path}.${key}`);
            return false;
        }

        if (typeof templateValue !== typeof objValue) {
            console.warn(`Type mismatch at ${path}.${key}: expected ${typeof templateValue}, got ${typeof objValue}`);
            return false;
        }

        if (templateValue !== null && typeof templateValue === 'object' && !Array.isArray(templateValue)) {
            if (!validateObjectStructure(objValue, templateValue, `${path}.${key}`)) {
                return false;
            }
        }
    }

    return true;
}

function updatePageSetting() {
    try {
        parseSideBarParam();
        const savedSettings = localStorage.getItem('pageSettings');
        if (savedSettings === null) return;

        let jsonSettings: PageSettings;
        jsonSettings = JSON.parse(savedSettings);

        const template = new PageSettings();

        if (!validateObjectStructure(jsonSettings, template)) {
            console.warn('PageSettings structure mismatch, clearing old cache');
            localStorage.removeItem('pageSettings');
            localStorage.removeItem('needRestore');
            return;
        }
    } catch (error) {
        localStorage.removeItem('pageSettings');
        localStorage.removeItem('needRestore');
        console.warn('json parse failed,error:',error);
    }
}

export function pageInit() {
    updatePageSetting();
    const canvasSizeSelect = document.getElementById('canvas-size-select') as HTMLSelectElement;
    canvasSizeSelect.innerHTML = '';
    addCanvasSizeOption('full-screen', 'windowSizeOption');
    addCanvasSizeOption('2048x1440', 'fixedSizeOption');
    const antiAliasSwitchSelect = document.getElementById('antialias-switch-select') as HTMLSelectElement;
    antiAliasSwitchSelect.innerHTML = '';
    addSwitchOption('antialias-switch-select', 'On', "antialiasOnOption");
    addSwitchOption('antialias-switch-select', 'Off', "antialiasOffOption");
    const strokeSwitchSelect = document.getElementById('stroke-switch-select') as HTMLSelectElement;
    strokeSwitchSelect.innerHTML = '';
    addSwitchOption('stroke-switch-select', 'On', "strokeOnOption");
    addSwitchOption('stroke-switch-select', 'Off', "strokeOffOption");
    addGraphicTypeOptions();

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
        if (queryString !== '') {
            param = UrlParamsManager.splitEngineParams(UrlParamsManager.getUrlParams());

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
        const threadTypeSelect = document.getElementById('thread-type-select') as HTMLSelectElement;
        if (!threadTypeSelect) return;

        const engineConfig = config.engineVersion[engineType as keyof typeof config.engineVersion];
        if (!engineConfig || !engineConfig.versions[version]) return;

        const threadTypes = engineConfig.versions[version];

        threadTypeSelect.innerHTML = '';

        threadTypes.forEach(type => {
            const option = document.createElement('option');
            option.value = type;
            option.setAttribute('data-locale', `${type}Option`);
            threadTypeSelect.appendChild(option);
        });

        if (threadTypes.length === 1) {
            threadTypeSelect.value = threadTypes[0];
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

    if (localStorage.getItem('needRestore') === 'true') {
        restorePageSettings();
    }
    if (queryString !== '') {
        try {
            param = UrlParamsManager.splitEngineParams(UrlParamsManager.getUrlParams());
            if (param['engine']) {
                engineTypeSelect.value = param['engine'].toString().toLowerCase();
            }
            if (param['version']) {
                const versionParam = param['version'].toString().toLowerCase();
                const options = versionSelect.options;
                for (let i = 0; i < options.length; i++) {
                    if (options[i].value.toLowerCase() === versionParam) {
                        versionSelect.value = options[i].value;
                        break;
                    }
                }
            }
            if (param['threadType']) {
                const threadTypeSelect = document.getElementById('thread-type-select') as HTMLSelectElement;
                threadTypeSelect.value = param['threadType'].toString();
            }
        } catch (error) {
            throw new Error(`The url parameter is incorrect: ${error}`);
        }
    } else {
        param = {};
        const threadTypeSelect = document.getElementById('thread-type-select') as HTMLSelectElement;
        param['engine'] = `${engineTypeSelect.value}-${versionSelect.value}-${threadTypeSelect.value}`;
        UrlParamsManager.setUrlParams(param);
    }
    if (engineTypeSelect.value === 'tgfx') {
        enumEngineType = EnumEngineType.tgfx;
    } else if (engineTypeSelect.value === 'skia') {
        enumEngineType = EnumEngineType.skia;
    }

    triggerLanguageChange();
    savePageSettings();
}


class PageSettings {
    configParams: {
        canvasSize: string;
        startCount: number;
        stepCount: number;
        maxShapes: number;
        minFPS: number;
    } = {
        canvasSize: 'full-screen',
        startCount: 1,
        stepCount: 1000,
        maxShapes: 1000000,
        minFPS: 60.0
    };
    graphicType: {
        options: string[];
        selected: string;
    } = {
        options: ['Rect', 'Circle', 'Oval', 'RRect'],
        selected: 'Rect'
    };
    language: {
        options: string[];
        selected: string;
    } = {
        options: ["auto", "zh", "en"],
        selected: 'auto'
    };
    antiAlias: {
        option: boolean;
    } = {
        option: true
    };
    stroke: {
        option: boolean;
    } = {
        option: false
    };
}

function savePageSettings() {
    const engineTypeSelect = document.getElementById('engine-type-select') as HTMLSelectElement;
    const engineType = {
        options: Array.from(engineTypeSelect.options).map(option => option.value),
        selected:
        engineTypeSelect.value
    };

    const versionSelect = document.getElementById('engine-version') as HTMLSelectElement;
    const engineVersion = {
        options: Array.from(versionSelect.options).map(option => option.value),
        selected: versionSelect.value
    };

    const threadSelect = document.getElementById('thread-type-select') as HTMLSelectElement;
    const threadType = {
        options: Array.from(threadSelect.options).map(option => option.value),
        selected: threadSelect.value
    };

    const configParams = {
        canvasSize: (document.getElementById('canvas-size-select') as HTMLSelectElement).value,
        startCount: Number((document.getElementById('startCount') as HTMLInputElement).value),
        stepCount: Number((document.getElementById('stepCount') as HTMLInputElement).value),
        maxShapes: Number((document.getElementById('maxShapes') as HTMLInputElement).value),
        minFPS: Number((document.getElementById('minFPS') as HTMLInputElement).value)
    };

    const graphicTypeSelect = document.getElementById('graphic-type-select') as HTMLSelectElement;
    const graphicType = {
        options: Array.from(graphicTypeSelect.options).map(option => option.value),
        selected: graphicTypeSelect.value
    };

    const languageSelect = document.getElementById('language-type') as HTMLSelectElement;
    const language = {
        options: Array.from(languageSelect.options).map(option => option.value),
        selected: languageSelect.value
    };

    const antiAliasSwitch = document.getElementById('antialias-switch-select') as HTMLInputElement;
    const antiAliasSwitchValue = antiAliasSwitch.value;

    const antiAliasValue = antiAliasSwitchValue === 'On';
    const antiAlias = {
        option: antiAliasValue
    }

    const strokeSwitch = document.getElementById('stroke-switch-select') as HTMLInputElement;
    const strokeSwitchValue = strokeSwitch.value;

    const strokeValue = strokeSwitchValue === 'On';
    const stroke = {
        option: strokeValue
    }


    const settings: PageSettings = {
        configParams,
        graphicType,
        language,
        antiAlias,
        stroke
    };

    localStorage.setItem('pageSettings', JSON.stringify(settings));
    localStorage.setItem('needRestore', 'true');
    let engineDir = `${engineVersionInfo['engineVersion'][engineType.selected].savePath}${engineType.selected}-${engineVersion.selected}-${threadType.selected}`
    localStorage.setItem('engineDir', engineDir);
}


export function pageRestart() {
    savePageSettings();
    location.reload();
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

        const canvasSizes = document.getElementById('canvas-size-select') as HTMLSelectElement;
        if (canvasSizes && configParams.canvasSize !== undefined) {
            canvasSizes.value = configParams.canvasSize;
        }

        const graphicTypeSelect = document.getElementById('graphic-type-select') as HTMLSelectElement;
        if (graphicTypeSelect && settings.graphicType.selected !== undefined) {
            graphicTypeSelect.value = settings.graphicType.selected;
        }

        const languageSelect = document.getElementById('language-type') as HTMLSelectElement;
        if (languageSelect && settings.language.selected) {
            languageSelect.value = settings.language.selected;
        }

        const antiAliasSwitchSelect = document.getElementById('antialias-switch-select') as HTMLSelectElement;
        if (antiAliasSwitchSelect && settings.antiAlias.option !== undefined) {
            antiAliasSwitchSelect.value = settings.antiAlias.option ? 'On' : 'Off';
        }

        const strokeSwitchSelect = document.getElementById('stroke-switch-select') as HTMLSelectElement;
        if (strokeSwitchSelect && settings.stroke?.option !== undefined) {
            strokeSwitchSelect.value = settings.stroke.option ? 'On' : 'Off';
        }

    } catch (error) {
        console.error('Failed to restore page configuration:', error);
    }
}


function setDefaultValues() {
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

    const graphicTypeSelect = document.getElementById('graphic-type-select') as HTMLSelectElement;
    graphicTypeSelect.value = graphicTypeStr[0];

    const antiAliasSwitchSelect = document.getElementById('antialias-switch-select') as HTMLSelectElement;
    antiAliasSwitchSelect.value = 'Off';

    const strokeSwitchSelect = document.getElementById('stroke-switch-select') as HTMLSelectElement;
    strokeSwitchSelect.value = 'Off';
}


export function getEngineDir(): string {
    const versionSelect = document.getElementById('engine-version') as HTMLSelectElement;
    const version = versionSelect.value;
    const engineType = (document.getElementById('engine-type-select') as HTMLSelectElement).value;
    const threadType = (document.getElementById('thread-type-select') as HTMLSelectElement).value;
    if (version !== "" && threadType !== "" && engineType !== "") {
        return `${engineVersionInfo['engineVersion'][engineType].savePath}${engineType}-${version}-${threadType}`;
    }
    return "";
}


function handleConfigParamChange(event: Event) {
    const target = event.target as HTMLInputElement;
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
    savePageSettings();
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
    const target = event.target as HTMLSelectElement;
    let graphicType = target.value;

    savePageSettings();
    setGraphicType(graphicType);
}

function handleEngineTypeChange(event: Event) {
    const target = event.target as HTMLSelectElement;
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
        handleEngineVersionChange({ target: engineVersionSelect } as unknown as Event);
    }
}

function handleEngineVersionChange(event: Event) {
    const target = event.target as HTMLSelectElement;
    const engineType = (document.getElementById('engine-type-select') as HTMLSelectElement).value;
    const selectedVersion = target.value;
    if (!selectedVersion) {
        return;
    }
    const threads = engineVersionInfo.engineVersion[engineType].versions[selectedVersion];
    const threadTypeSelect = document.getElementById('thread-type-select') as HTMLSelectElement;
    if (!threadTypeSelect) {
        return;
    }

    threadTypeSelect.innerHTML = '';

    threads.forEach(threadType => {
        const option = document.createElement('option');
        option.value = threadType;
        threadTypeSelect.appendChild(option);
    });

    let defaultThreadType = threads.includes('mt') ? 'mt' : threads[0];
    threadTypeSelect.value = defaultThreadType;
    handleThreadTypeChange({ target: threadTypeSelect } as unknown as Event);
}


function handleThreadTypeChange(event: Event) {
    const target = event.target as HTMLSelectElement;
    const engineType = (document.getElementById('engine-type-select') as HTMLSelectElement).value;
    const engineVersion = (document.getElementById('engine-version') as HTMLSelectElement).value;
    const threadType = target.value;
    const engineDir = `${engineVersionInfo.engineVersion[engineType].savePath}${engineType}-${engineVersion}-${threadType}`;
    localStorage.setItem('engineDir', engineDir);
    const param: ParamsObject = {};
    param["engine"] = `${engineType}-${engineVersion}-${threadType}`;
    UrlParamsManager.setUrlParams(param);
    pageRestart();
}


function handleCanvasSizeChange(event: Event) {
    const target = event.target as HTMLSelectElement;
    const container = document.getElementById('container') as HTMLDivElement;
    const canvas = document.getElementById('benchmark') as HTMLCanvasElement;
    const canvasWrapper = document.getElementById('canvasWrapper');

    if (target.value === 'full-screen') {
        // 切换为全屏模式
        canvas.classList.remove('fixed-size');
        canvas.classList.add('full-screen');
        canvas.style.width = '';
        canvas.style.height = '';
        canvas.style.border = '';
        canvas.style.boxSizing = '';
        canvas.style.backgroundColor = '';
        container.style.backgroundColor = '';
        container.style.display = '';
        container.style.justifyContent = '';
        container.style.alignItems = '';
        // 隐藏四角标记
        if (canvasWrapper) {
            canvasWrapper.classList.remove('show-corners');
        }
    } else if (target.value === '2048x1440') {
        // 切换为固定尺寸模式
        canvas.classList.remove('full-screen');
        canvas.classList.add('fixed-size');
        container.style.backgroundColor = '#262E3C';
        container.style.display = 'flex';
        container.style.justifyContent = 'center';
        container.style.alignItems = 'center';

        const scaleFactor = window.devicePixelRatio;
        canvas.width = 2048;
        canvas.height = 1440;

        const scaleWidth = canvas.width / scaleFactor;
        const scaleHeight = canvas.height / scaleFactor;

        canvas.style.setProperty('width', scaleWidth + 'px', 'important');
        canvas.style.setProperty('height', scaleHeight + 'px', 'important');

        // 显示四角标记（现在通过 CSS 自动跟随 canvas）
        if (canvasWrapper) {
            canvasWrapper.classList.add('show-corners');
        }
    }
    updateSize(shareData);
    localStorage.setItem('canvasSize', target.value);
    savePageSettings();

    const resolutionSpan = document.querySelector('.resolution');
    if (resolutionSpan) {
        resolutionSpan.textContent = `${canvas.width} x ${canvas.height}`;
    }

    if (shareData.baseView) {
        shareData.baseView.restartDraw();
    }
}

function handleAntiAliasChange(event: Event) {
    const target = event.target as HTMLInputElement;
    if (shareData.baseView) {
        const antiAlia: boolean = target.value == 'On';
        shareData.baseView.setAntiAlias(antiAlia);
    }
    savePageSettings();
}

function handleStrokeChange(event: Event) {
    const target = event.target as HTMLInputElement;
    if (shareData.baseView) {
        const stroke: boolean = target.value == 'On';
        shareData.baseView.setStroke(stroke);
    }
    savePageSettings();
}

function handleLanguageTypeChange(event: Event) {
    savePageSettings();
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
                updateProgress(getRandomInt(50, 70));
                return path;
            },
            onRuntimeInitialized: () => {
                updateProgress(getRandomInt(71, 90));
            }
        };
        if (type === 'mt') {
            moduleConfig['mainScriptUrlOrBlob'] = `${engineDir}.js`;
        }
        const module = await Benchmark.default(moduleConfig);

        if (enumEngineType === EnumEngineType.tgfx) {
            shareData.BenchmarkModule = module;
            TGFXBind(shareData.BenchmarkModule);
            let tgfxView: TGFXBaseView;
            if (type === 'mt') {
                tgfxView = shareData.BenchmarkModule.TGFXThreadsView.MakeFrom('#benchmark');
            } else {
                tgfxView = shareData.BenchmarkModule.TGFXView.MakeFrom('#benchmark');
            }

            shareData.baseView = tgfxView;
        } else if (enumEngineType === EnumEngineType.skia) {
            let skiaView: SkiaView = module.SkiaView.MakeFrom('#benchmark');
            shareData.baseView = skiaView;
        }
        updateProgress(getRandomInt(91, 99));
        shareData.baseView.showPerfData(false);
        updateProgress(100);
        hideProgress();
        setDrawParamFromPageSettings();
        loadCanvasSizeAndTriggerChange(shareData);
        startDraw(shareData);
    } catch (error) {
        console.error(error);
        throw new Error(`Failed to load ${type} version. Please check the wasm file path!`);
    }
}


export function bindEventListeners() {
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

    const graphicTypeSelect = document.getElementById('graphic-type-select');
    if (graphicTypeSelect) {
        graphicTypeSelect.addEventListener('change', handleGraphicTypeChange);
    }

    const engineTypeSelect = document.getElementById('engine-type-select');
    if (engineTypeSelect) {
        engineTypeSelect.addEventListener('change', handleEngineTypeChange);
    }

    const engineVersionSelect = document.getElementById('engine-version');
    if (engineVersionSelect) {
        engineVersionSelect.addEventListener('change', handleEngineVersionChange);
    }

    const threadTypeSelect = document.getElementById('thread-type-select');
    if (threadTypeSelect) {
        threadTypeSelect.addEventListener('change', handleThreadTypeChange);
    }

    const canvasSizeSelect = document.getElementById('canvas-size-select');
    if (canvasSizeSelect) {
        canvasSizeSelect.addEventListener('change', handleCanvasSizeChange);
    }

    const antiAliasSelect = document.getElementById('antialias-switch-select');
    if (antiAliasSelect) {
        antiAliasSelect.addEventListener('change', handleAntiAliasChange);
    }

    const strokeSelect = document.getElementById('stroke-switch-select');
    if (strokeSelect) {
        strokeSelect.addEventListener('change', handleStrokeChange);
    }

    const languageTypeSelect = document.getElementById('language-type');
    if (languageTypeSelect) {
        languageTypeSelect.addEventListener('change', handleLanguageTypeChange);
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

    static splitEngineParams(param: ParamsObject): ParamsObject {
        const params: ParamsObject = {};
        const engineParts = param['engine'].toString().toLowerCase().split('-');
        if (engineParts.length !== 3) {
            console.error('params.engine is invalid,example: engine-version-threadType');
            return;
        }
        const engineType = engineParts[0];
        const version = engineParts[1];
        const threadType = engineParts[2];
        params['engine'] = engineType;
        params['version'] = version;
        params['threadType'] = threadType;

        return params;
    }

    static validateUrlParams(params: ParamsObject): boolean {
        if (params['engine'] === undefined) {
            return false;
        }
        if (params['engine'] !== undefined) {
            const engineParts = params['engine'].toString().toLowerCase().split('-');
            if (engineParts.length !== 3) {
                console.error('params.engine is invalid,example: engine-version-threadType');
                return;
            }
            const engineType = engineParts[0];
            const version = engineParts[1];
            const threadType = engineParts[2];

            if (engineVersionInfo.engineVersion[engineType] === undefined) {
                console.error('engineType is invalid,example: tgfx/skia');
                return false;
            }

            if (engineVersionInfo.engineVersion[engineType].versions[version] === undefined) {
                console.error('engine version is invalid');
                return false;
            }

            if (engineVersionInfo.engineVersion[engineType].versions[version].includes(threadType) === false) {
                console.error('threadType is invalid');
                return false;
            }
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

export function setDrawParamFromPageSettings() {

    const savedSettings = localStorage.getItem('pageSettings');
    if (savedSettings === null) return;

    let jsonSettings: PageSettings;
    try {
        jsonSettings = JSON.parse(savedSettings);
    } catch (error) {
        console.error('Page parameter parsing failed:', error);
        return;
    }
    let drawingParam = jsonSettings.configParams;
    drawParam.startCount = Number(drawingParam.startCount);
    drawParam.stepCount = Number(drawingParam.stepCount);
    drawParam.maxCount = Number(drawingParam.maxShapes);
    drawParam.minFPS = Number(drawingParam.minFPS);
    shareData.baseView.updateDrawParam(drawParam);
    setGraphicType(jsonSettings.graphicType.selected);
    shareData.baseView.setAntiAlias(jsonSettings.antiAlias.option);
    if (jsonSettings.stroke?.option !== undefined) {
        shareData.baseView.setStroke(jsonSettings.stroke.option);
    }
}

function webUpdateGraphicType(type: number) {
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

        const graphicTypeSelect = document.getElementById('graphic-type-select') as HTMLSelectElement;
        if (!graphicTypeSelect) {
            console.error('graphic-type-select not found');
            return;
        }
        graphicTypeSelect.value = typeStr;
        // 同步更新自定义下拉框的显示
        if (typeof (window as any).syncCustomSelectValue === 'function') {
            (window as any).syncCustomSelectValue(graphicTypeSelect);
        }
        savePageSettings();
    } catch (error) {
        console.error('Error updating graphic type:', error);
    }
}

export function setupCoordinateConversion(canvasId: string, showSideBar: boolean) {
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


function loadCanvasSizeAndTriggerChange(shareData: ShareData): void {
    try {
        const savedSize = localStorage.getItem('canvasSize');
        const sizeSelect = document.getElementById('canvas-size-select') as HTMLSelectElement;
        if (savedSize && sizeSelect) {
            sizeSelect.value = savedSize;
            handleCanvasSizeChange({
                target: sizeSelect,
                isTrusted: true
            } as unknown as Event);
        } else {
            updateSize(shareData);
        }
    } catch (error) {
        console.error('loadCanvasSizeAndTriggerChange:', error);
    }
}

export function triggerLanguageChange(): void {
    const languageSelect = document.getElementById('language-type') as HTMLSelectElement;
    if (!languageSelect) {
        console.error('find languageSelect failed');
        return;
    }
    const event = new Event('change', {
        bubbles: true,
        cancelable: true
    });
    languageSelect.dispatchEvent(event);
}


function addCanvasSizeOption(value: string, localeKey: string): void {
    const select = document.getElementById('canvas-size-select') as HTMLSelectElement;
    if (!select) {
        console.error('find canvas-size-select failed');
        return;
    }
    const option = document.createElement('option');
    option.value = value;
    option.setAttribute('data-locale', localeKey);

    const savedSize = localStorage.getItem('canvasSize');

    if (savedSize === value) {
        option.selected = true;
    }

    select.appendChild(option);
}

function addSwitchOption(elemId: string, value: string, localeKey: string): void {
    const select = document.getElementById(elemId) as HTMLSelectElement;
    if (!select) {
        console.error('find switch-select failed');
        return;
    }
    const option = document.createElement('option');
    option.value = value;
    option.setAttribute('data-locale', localeKey);

    select.appendChild(option);
}


function addGraphicTypeOptions() {
    const select = document.getElementById('graphic-type-select') as HTMLSelectElement;
    if (!select) return;
    select.innerHTML = '';
    graphicTypeStr.forEach(type => {
        const option = document.createElement('option');
        option.value = type;
        option.textContent = type;
        option.setAttribute('data-locale', `${type.toLowerCase()}Option`);
        select.appendChild(option);
    });
}