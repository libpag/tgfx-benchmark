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

export class TGFXBaseView {
    public updateSize: (devicePixelRatio: number) => void;
    public startDraw: () => void;
    public init: () => void;
}

export class ShareData {
    public BenchmarkModule: types.TGFX = null;
    public tgfxBaseView: TGFXBaseView = null;
    public resized: boolean = false;
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

export function onresizeEvent(shareData: ShareData) {
    if (!shareData.tgfxBaseView) {
        return;
    }
    shareData.resized = true;
}

const canvasDefaultWidth:number = 2048;
const canvasDefaultHeight:number = 1440;

export function setCanvasDefaultSize(shareData: ShareData) {
    const container = document.getElementById('container') as HTMLDivElement;
    const canvas = document.getElementById('benchmark') as HTMLCanvasElement;

    container.style.backgroundColor = '#2c2c2c';
    canvas.style.border = '10px solid #d2d2d2';
    canvas.style.boxSizing = 'content-box';
    canvas.style.backgroundColor = '#ffffff';
    const scaleFactor = window.devicePixelRatio;
    canvas.width = canvasDefaultWidth;
    canvas.height = canvasDefaultHeight;

    const scaleWidth = canvas.width / scaleFactor;
    const scaleHeight = canvas.height / scaleFactor;

    canvas.style.width = scaleWidth + 'px';
    canvas.style.height = scaleHeight + 'px';
    container.style.display = 'flex';
    container.style.justifyContent = 'center';
    container.style.alignItems = 'center';
    shareData.tgfxBaseView.updateSize(scaleFactor);
}


export function setupCoordinateConversion(canvasId: string) {
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
            const convertedEvent = convertCoordinates(e);
            console.log("convertedEvent click: " + convertedEvent.clientX + ", " + convertedEvent.clientY);
            console.log("e click: " + e.clientX + ", " + e.clientY);

            Object.defineProperty(e, 'clientX', {
                value: convertedEvent.clientX,
                writable: true
            });
            Object.defineProperty(e, 'clientY', {
                value: convertedEvent.clientY,
                writable: true
            });
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
            const convertedEvent = convertCoordinates(e);
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


function convertCoordinates(e: MouseEvent) {
    let offsetX = 0;
    let offsetY = 0;
    const benchmark = document.getElementById('benchmark');
    const container = document.getElementById('container');

    if (benchmark && container) {
        const benchmarkWidth = benchmark.clientWidth;
        const benchmarkHeight = benchmark.clientHeight;
        const containerWidth = container.clientWidth;
        const containerHeight = container.clientHeight;

        if (benchmarkWidth !== containerWidth && benchmarkHeight !== containerHeight) {
            offsetX = (containerWidth - benchmarkWidth) / 2;
            offsetY = (containerHeight - benchmarkHeight) / 2;
        }
    }
    return {
        clientX: (e.clientX - offsetX),
        clientY: (e.clientY - offsetY)
    };
}