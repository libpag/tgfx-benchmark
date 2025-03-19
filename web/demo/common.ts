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
    public restartDraw: () => void;
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

export async function parseSideBarParam(): Promise<SideBarConfig> {
    try {
        const response = await fetch('static/sideBarConfigParam.json');
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        const config = await response.json() as SideBarConfig;
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
    } else {
        restoreEngineType();
    }
    parseSideBarParam().then(config => {
        const tgfxRadioItem = document.getElementById('tgfx-radio-item');
        const skiaRadioItem = document.getElementById('skia-radio-item');
        const engineTypeInputs = document.getElementsByName('engine-type') as NodeListOf<HTMLInputElement>;
        const versionSelect = document.getElementById('engine-version') as HTMLSelectElement;
        const engineTypeOptions = document.getElementById('engine-type-radio-options');

        function handleEngineVisibility() {
            if (!config.engineVersion.tgfx && tgfxRadioItem) {
                tgfxRadioItem.style.display = 'none';
            }
            if (!config.engineVersion.skia && skiaRadioItem) {
                skiaRadioItem.style.display = 'none';
            }
            let selectedEngine = '';
            if (config.engineVersion.tgfx && !config.engineVersion.skia) {
                selectedEngine = 'tgfx';
            } else if (!config.engineVersion.tgfx && config.engineVersion.skia) {
                selectedEngine = 'skia';
            }
            if (selectedEngine) {
                const radio = document.querySelector(`input[name="engine-type"][value="${selectedEngine}"]`) as HTMLInputElement;
                if (radio) {
                    radio.checked = true;
                }
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

        const checkedEngine = Array.from(engineTypeInputs).find(input => input.checked);
        if (checkedEngine) {
            updateVersionSelect(checkedEngine.value);
            if (versionSelect) {
                updateThreadTypeOptions(checkedEngine.value, versionSelect.value);
            }
        }

        if (engineTypeOptions) {
            engineTypeOptions.addEventListener('change', (e) => {
                const target = e.target as HTMLInputElement;
                if (target.type === 'radio' && target.name === 'engine-type' && target.checked) {
                    updateVersionSelect(target.value);
                    if (versionSelect) {
                        updateThreadTypeOptions(target.value, versionSelect.value);
                    }
                }
            });
        }

        if (versionSelect) {
            versionSelect.addEventListener('change', (e) => {
                const target = e.target as HTMLSelectElement;
                const checkedEngine = Array.from(engineTypeInputs).find(input => input.checked);
                if (checkedEngine) {
                    updateThreadTypeOptions(checkedEngine.value, target.value);
                }
            }, true);
        }

        if (localStorage.getItem('needRestore') === 'true') {
            restorePageSettings();
            localStorage.removeItem('needRestore');
        }

    }).catch(error => {
        console.error('加载配置失败:', error);
    });
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
    const engineTypeInputs = document.getElementsByName('engine-type') as NodeListOf<HTMLInputElement>;
    const engineType = {
        options: Array.from(engineTypeInputs).map(input => input.value),
        selected: Array.from(engineTypeInputs).find(input => input.checked)?.value || ''
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
    const engineTypeRadio = document.querySelector(
        `input[name="engine-type"][value="${settings.engineType.selected}"]`
    ) as HTMLInputElement;
    if (engineTypeRadio) {
        engineTypeRadio.checked = true;
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
    const tgfxRadio = document.querySelector('input[name="engine-type"][value="tgfx"]') as HTMLInputElement;
    if (tgfxRadio) {
        tgfxRadio.checked = true;
    }
    const mtRadio = document.querySelector('input[name="thread-type"][value="mt"]') as HTMLInputElement;
    if (mtRadio) {
        mtRadio.checked = true;
    }
    const defaultParams = {
        startCount: 100,
        stepCount: 40,
        maxShapes: 300000,
        minFPS: 60
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
