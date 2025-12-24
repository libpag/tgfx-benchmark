const LANGUAGE_KEY = 'preferred_language';
const DEFAULT_LANGUAGE = 'auto';

function initLanguage() {
    const savedLanguage = localStorage.getItem(LANGUAGE_KEY) || DEFAULT_LANGUAGE;

    const languageSelect = document.getElementById('language-type');
    if (languageSelect) {
        languageSelect.value = savedLanguage;
    }

    applyLanguage(savedLanguage);
}

function handleLanguageChange(select) {
    const selectedLanguage = select;
    localStorage.setItem(LANGUAGE_KEY, selectedLanguage);
    applyLanguage(selectedLanguage);
}

function applyLanguage(language) {
    if (language === 'auto') {
        const browserLang = navigator.language.toLowerCase();
        language = browserLang.startsWith('zh') ? 'zh' : 'en';
    }
    initResourceBundle(language);
}

function checkBrowser() {
    const browserWarning = document.getElementById('browser-warning');
    const maskOverlay = document.getElementById('mask-overlay');

    function isMobile() {
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
    }

    function isChromiumBased() {
        const ua = navigator.userAgent;
        return ua.includes('Chrome') || ua.includes('Edge');
    }

    function isDesktop() {
        return !isMobile();
    }

    let isSupported = true;
    if (!isDesktop() || !isChromiumBased()) {
        browserWarning.classList.remove('hidden');
        maskOverlay.classList.remove('hidden');
        setTimeout(() => {
            browserWarning.classList.add('show');
        }, 100);
        isSupported = false;
    }
    localStorage.setItem('isSupported', JSON.stringify(isSupported));
    return isSupported;
}


function getLocaleBundle(locale) {
    if (locale === "zh") {
        return LocaleMaps_zh_CN;
    }
    return LocaleMaps_en_US;
}

function initResourceBundle(locale) {
    const resourceBundle = getLocaleBundle(locale);
    const resElms = document.querySelectorAll("[data-locale]");
    for (let n = 0; n < resElms.length; n++) {
        const resEl = resElms[n];
        // Get the resource key from the element.
        const resKey = resEl.getAttribute("data-locale");
        if (resKey) {
            // Get all the resources that start with the key.
            for (const key in resourceBundle) {
                if (key.indexOf(resKey) === 0) {
                    const resValue = resourceBundle[key];
                    if (key.length === resKey.length) {
                        resEl.innerHTML = resValue;
                    } else if ("." === key.charAt(resKey.length)) {
                        const attrKey = key.substring(resKey.length + 1);
                        resEl[attrKey] = resValue;
                    }
                }
            }
        }
    }
    // 更新自定义下拉框的显示文本
    if (typeof updateCustomSelectsText === 'function') {
        updateCustomSelectsText();
    }
    return resourceBundle;
}

function updatePerfInfo(fps, drawTime, drawCount, maxDrawCountReached) {
    const fpsElement = document.querySelector('.fps');
    const fpsParagraph = document.querySelector('.fpsP');
    const timeElement = document.querySelector('.time');
    const timeParagraph = document.querySelector('.timeP');
    const countElement = document.querySelector('.count');
    // 固定使用 #CCE1FF 颜色，不再根据数值变化
    const fixedColor = '#CCE1FF';
    if (fpsElement && fpsParagraph) {
        fpsElement.textContent = Number(fps).toFixed(1);
        fpsElement.style.color = fixedColor;
        fpsParagraph.style.color = fixedColor;
    }
    if (timeElement && timeParagraph) {
        timeElement.textContent = Number(drawTime).toFixed(1);
        timeElement.style.color = fixedColor;
        timeParagraph.style.color = fixedColor;
    }
    const targetFPS = parseFloat(document.getElementById('minFPS').value) || 60;

    drawCount = Number(drawCount).toLocaleString('en-US');
    maxDrawCountReached = !!maxDrawCountReached;
    if (maxDrawCountReached) {
        if (countElement) countElement.textContent = `[${drawCount}]`;
    } else {
        if (countElement) countElement.textContent = drawCount;
    }
}

window.updatePerfInfo = updatePerfInfo;

// ===== 自定义下拉框功能 =====
function initCustomSelects() {
    const selects = document.querySelectorAll('select');
    
    selects.forEach(select => {
        // 跳过已经处理过的 select
        if (select.parentElement && select.parentElement.classList.contains('custom-select-wrapper')) {
            return;
        }
        
        // 创建自定义下拉框包装器
        const wrapper = document.createElement('div');
        wrapper.className = 'custom-select-wrapper';
        
        // 创建触发器（显示当前选中值）
        const trigger = document.createElement('div');
        trigger.className = 'custom-select-trigger';
        
        // 创建选项容器
        const optionsContainer = document.createElement('div');
        optionsContainer.className = 'custom-select-options';
        
        // 获取当前选中的选项文本
        const selectedOption = select.options[select.selectedIndex];
        trigger.textContent = selectedOption ? selectedOption.textContent : '';
        
        // 为每个选项创建自定义选项元素
        Array.from(select.options).forEach((option, index) => {
            const customOption = document.createElement('div');
            customOption.className = 'custom-select-option';
            if (index === select.selectedIndex) {
                customOption.classList.add('selected');
            }
            customOption.textContent = option.textContent;
            customOption.dataset.value = option.value;
            customOption.dataset.index = index;
            
            customOption.addEventListener('click', (e) => {
                e.stopPropagation();
                
                // 更新原生 select 的值
                select.selectedIndex = index;
                select.value = option.value;
                
                // 更新触发器文本
                trigger.textContent = option.textContent;
                
                // 更新选中状态
                optionsContainer.querySelectorAll('.custom-select-option').forEach(opt => {
                    opt.classList.remove('selected');
                });
                customOption.classList.add('selected');
                
                // 关闭下拉框
                wrapper.classList.remove('open');
                
                // 触发原生 change 事件
                const event = new Event('change', { bubbles: true });
                select.dispatchEvent(event);
            });
            
            optionsContainer.appendChild(customOption);
        });
        
        // 点击触发器切换下拉框
        trigger.addEventListener('click', (e) => {
            e.stopPropagation();
            
            // 关闭其他打开的下拉框
            document.querySelectorAll('.custom-select-wrapper.open').forEach(w => {
                if (w !== wrapper) {
                    w.classList.remove('open');
                }
            });
            
            wrapper.classList.toggle('open');
        });
        
        // 将原生 select 包装到自定义结构中
        select.parentNode.insertBefore(wrapper, select);
        wrapper.appendChild(trigger);
        wrapper.appendChild(optionsContainer);
        wrapper.appendChild(select);
    });
    
    // 点击外部关闭所有下拉框
    document.addEventListener('click', () => {
        document.querySelectorAll('.custom-select-wrapper.open').forEach(w => {
            w.classList.remove('open');
        });
    });
}

// 更新自定义下拉框选项（当原生 select 选项改变时调用）
function updateCustomSelect(selectElement) {
    const wrapper = selectElement.closest('.custom-select-wrapper');
    if (!wrapper) return;
    
    const trigger = wrapper.querySelector('.custom-select-trigger');
    const optionsContainer = wrapper.querySelector('.custom-select-options');
    
    // 清空现有选项
    optionsContainer.innerHTML = '';
    
    // 重新创建选项
    Array.from(selectElement.options).forEach((option, index) => {
        const customOption = document.createElement('div');
        customOption.className = 'custom-select-option';
        if (index === selectElement.selectedIndex) {
            customOption.classList.add('selected');
        }
        customOption.textContent = option.textContent;
        customOption.dataset.value = option.value;
        customOption.dataset.index = index;
        
        customOption.addEventListener('click', (e) => {
            e.stopPropagation();
            selectElement.selectedIndex = index;
            selectElement.value = option.value;
            trigger.textContent = option.textContent;
            optionsContainer.querySelectorAll('.custom-select-option').forEach(opt => {
                opt.classList.remove('selected');
            });
            customOption.classList.add('selected');
            wrapper.classList.remove('open');
            const event = new Event('change', { bubbles: true });
            selectElement.dispatchEvent(event);
        });
        
        optionsContainer.appendChild(customOption);
    });
    
    // 更新触发器文本
    const selectedOption = selectElement.options[selectElement.selectedIndex];
    if (selectedOption) {
        trigger.textContent = selectedOption.textContent;
    }
}

// 同步自定义下拉框的显示值
function syncCustomSelectValue(selectElement) {
    const wrapper = selectElement.closest('.custom-select-wrapper');
    if (!wrapper) return;
    
    const trigger = wrapper.querySelector('.custom-select-trigger');
    const optionsContainer = wrapper.querySelector('.custom-select-options');
    
    // 更新触发器文本
    const selectedOption = selectElement.options[selectElement.selectedIndex];
    if (selectedOption) {
        trigger.textContent = selectedOption.textContent;
    }
    
    // 更新选中状态
    optionsContainer.querySelectorAll('.custom-select-option').forEach((opt, index) => {
        if (index === selectElement.selectedIndex) {
            opt.classList.add('selected');
        } else {
            opt.classList.remove('selected');
        }
    });
}

// 更新所有自定义下拉框的文本（语言切换时调用）
function updateCustomSelectsText() {
    document.querySelectorAll('.custom-select-wrapper').forEach(wrapper => {
        const select = wrapper.querySelector('select');
        const trigger = wrapper.querySelector('.custom-select-trigger');
        const optionsContainer = wrapper.querySelector('.custom-select-options');
        
        if (!select || !trigger || !optionsContainer) return;
        
        // 更新触发器文本
        const selectedOption = select.options[select.selectedIndex];
        if (selectedOption) {
            trigger.textContent = selectedOption.textContent;
        }
        
        // 更新所有选项文本
        const customOptions = optionsContainer.querySelectorAll('.custom-select-option');
        customOptions.forEach((customOption, index) => {
            if (select.options[index]) {
                customOption.textContent = select.options[index].textContent;
            }
        });
    });
}

window.initCustomSelects = initCustomSelects;
window.updateCustomSelect = updateCustomSelect;
window.syncCustomSelectValue = syncCustomSelectValue;
window.updateCustomSelectsText = updateCustomSelectsText;

document.addEventListener('DOMContentLoaded', checkBrowser);
document.addEventListener('DOMContentLoaded', initLanguage);
document.addEventListener('DOMContentLoaded', function () {
    const languageSelect = document.getElementById('language-type');
    languageSelect.addEventListener('change', function () {
        const selectedLanguage = this.value;
        handleLanguageChange(selectedLanguage);
    });
});

// 在页面加载完成后初始化自定义下拉框
document.addEventListener('DOMContentLoaded', function() {
    // 延迟初始化，确保其他脚本已经填充了 select 选项
    setTimeout(initCustomSelects, 500);
});

document.addEventListener('wheel', function (e) {
    if (e.ctrlKey) {
        e.preventDefault();
    }
}, {passive: false});
