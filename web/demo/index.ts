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
import {ShareData, updateSize, onresizeEvent, startDraw, pageInit, pageRestart} from "./common";

let shareData: ShareData;


async function loadModule(type: string) {
    try {

        shareData = new ShareData();
        const Benchmark = await import(`./wasm-${type}/benchmark.js`);
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
                const path = `./wasm-${type}/` + file;
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
            moduleConfig['mainScriptUrlOrBlob'] = `./wasm-${type}/benchmark.js`;
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
    const radio = event.target as HTMLInputElement;
    if (radio.checked) {
        const threadType = radio.value; // 'st' 或 'mt'
        localStorage.setItem('threadType', threadType);
        console.log(`Thread type changed to ${threadType}`);
        await loadModule(threadType);
        pageRestart();
    }
}

function bindEventListeners() {
    const radios = document.querySelectorAll('input[name="thread-type"]');
    radios.forEach(radio => {
        radio.addEventListener('change', handleThreadTypeChange);
    });

    const restartBtn = document.getElementById('restartBtn');
    if (restartBtn) {
        restartBtn.addEventListener('click', () => {
            if (!shareData.tgfxBaseView) {
                return;
            }
            shareData.tgfxBaseView.restartDraw();
        });
    }
}

function refreshThreadType() {
    const threadTypeRadios = document.querySelectorAll<HTMLInputElement>('input[name="thread-type"]');

    if (threadTypeRadios.length === 2) {
        const savedThreadType = localStorage.getItem('threadType') || 'mt';
        const targetRadio = document.querySelector<HTMLInputElement>(`input[name="thread-type"][value="${savedThreadType}"]`);
        if (targetRadio) {
            targetRadio.checked = true;
        }
    } else if (threadTypeRadios.length === 1) {
        threadTypeRadios[0].checked = true;
    } else {
        console.warn(`异常：发现 ${threadTypeRadios.length} 个 thread-type radio`);
    }
}

if (typeof window !== 'undefined') {
    window.onload = () => {
        pageInit();

        const savedThreadType = localStorage.getItem('threadType') || 'mt';
        const radio = document.querySelector(`input[name="thread-type"][value="${savedThreadType}"]`) as HTMLInputElement;
        if (radio) {
            radio.checked = true;
        }
        loadModule(savedThreadType);
        bindEventListeners();
    };
    window.onresize = () => {
        onresizeEvent(shareData);
        window.setTimeout(updateSize(shareData), 300);
    };
}