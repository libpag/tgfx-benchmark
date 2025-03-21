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
import {TGFXBind} from '../lib/tgfx';
import {
    ShareData,
    DataType,
    GraphicType,
    engineVensionInfo,
    updateSize,
    onresizeEvent,
    startDraw,
    pageInit,
    pageRestart,
    getEngineDir
} from "./common";

let shareData: ShareData;

function handleConfigParamChange(event: Event) {
    const target = event.target as HTMLInputElement;
    if (target.type !== 'number') {
        return;
    }
    if (!event.isTrusted) {
        return;
    }
    if (target.id === 'startCount') {
        shareData.tgfxBaseView.updateDrawParam(DataType.startCount, Number(target.value));
    } else if (target.id === 'stepCount') {
        shareData.tgfxBaseView.updateDrawParam(DataType.stepCount, Number(target.value));
    } else if (target.id === 'maxShapes') {
        shareData.tgfxBaseView.updateDrawParam(DataType.maxDrawCount, Number(target.value));
    } else if (target.id === 'minFPS') {
        shareData.tgfxBaseView.updateDrawParam(DataType.minFPS, Number(target.value));
    }
}

function handleGraphicTypeChange(event: Event) {
    const target = event.target as HTMLInputElement;
    if (target.type === 'radio' && target.name === 'graphic-type' && target.checked) {
        const graphicType = target.value;

        let type: GraphicType;
        switch (graphicType) {
            case 'rectangle':
                type = GraphicType.rectangle;
                break;
            case 'round':
                type = GraphicType.round;
                break;
            case 'roundedRectangle':
                type = GraphicType.roundedRectangle;
                break;
            case 'oval':
                type = GraphicType.oval;
                break;
            case 'simpleGraphicBlending':
                type = GraphicType.simpleGraphicBlending;
                break;
            case 'complexGraphics':
                type = GraphicType.complexGraphics;
                break;
            default:
                type = GraphicType.rectangle;
        }

        if (shareData.tgfxBaseView) {
            shareData.tgfxBaseView.updateGraphicType(type);
        }
    }
}

function handleEngineTypeChange(event: Event) {
    const target = event.target as HTMLSelectElement;
    let engineDir = "";
    const selectedEngine = target.value;
    const engineVersionSelect = document.getElementById('engine-version') as HTMLSelectElement;
    engineVersionSelect.innerHTML = '';
    const versions = engineVensionInfo.engineVersion[selectedEngine]?.versions || {};

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
    const engineConfig = engineVensionInfo.engineVersion[engineTypeSelect.value];
    if (engineConfig) {
        engineDir = `${engineConfig.savePath}benchmark-${selectedVersion}`;
        localStorage.setItem('engineDir', engineDir);
        pageRestart();
    } else {
        console.error('无效的引擎配置:', engineTypeSelect.value);
    }
}

async function loadModule(engineDir: string, type: string = "mt") {
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

        shareData.BenchmarkModule = await Benchmark.default(moduleConfig);
        console.log(`BenchmarkModule:${shareData.BenchmarkModule}`);
        TGFXBind(shareData.BenchmarkModule);

        let tgfxView = shareData.BenchmarkModule.TGFXThreadsView.MakeFrom('#benchmark');
        shareData.tgfxBaseView = tgfxView;

        var imagePath = "static/image/bridge.jpg";
        await tgfxView.setImagePath(imagePath);

        var fontPath = "static/font/NotoSansSC-Regular.otf";
        const fontBuffer = await fetch(fontPath).then((response) => response.arrayBuffer());
        const fontUIntArray = new Uint8Array(fontBuffer);
        var emojiFontPath = "static/font/NotoColorEmoji.ttf";
        const emojiFontBuffer = await fetch(emojiFontPath).then((response) => response.arrayBuffer());
        const emojiFontUIntArray = new Uint8Array(emojiFontBuffer);
        tgfxView.registerFonts(fontUIntArray, emojiFontUIntArray);
        updateSize(shareData);
        startDraw(shareData);
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

function bindEventListeners() {

    const threadTypeRadioGroup = document.querySelector('.radio-group-thread-type');
    if (threadTypeRadioGroup) {
        threadTypeRadioGroup.addEventListener('change', handleThreadTypeChange);
    }

    const restartBtn = document.getElementById('restartBtn');
    if (restartBtn) {
        restartBtn.addEventListener('click', () => {
            if (!shareData.tgfxBaseView) {
                return;
            }
            shareData.tgfxBaseView.restartDraw();
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

if (typeof window !== 'undefined') {
    window.onload = () => {
        pageInit();
        const isSupported = JSON.parse(localStorage.getItem('isSupported'));
        if (isSupported) {
            const engineDir = getEngineDir();
            if (engineDir === "") {
                throw "engineDir is None";
            }
            loadModule(engineDir);
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