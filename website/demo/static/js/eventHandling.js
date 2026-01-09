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
    const fixedColor = '#ffffff';
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

function initCustomSelects() {
    const selects = document.querySelectorAll('select');
    
    selects.forEach(select => {
        if (select.parentElement && select.parentElement.classList.contains('custom-select-wrapper')) {
            return;
        }
        
        const wrapper = document.createElement('div');
        wrapper.className = 'custom-select-wrapper';
        
        const trigger = document.createElement('div');
        trigger.className = 'custom-select-trigger';
        
        const optionsContainer = document.createElement('div');
        optionsContainer.className = 'custom-select-options';
        
        const selectedOption = select.options[select.selectedIndex];
        trigger.textContent = selectedOption ? selectedOption.textContent : '';
        
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
                
                select.selectedIndex = index;
                select.value = option.value;
                
                trigger.textContent = option.textContent;
                
                optionsContainer.querySelectorAll('.custom-select-option').forEach(opt => {
                    opt.classList.remove('selected');
                });
                customOption.classList.add('selected');
                
                wrapper.classList.remove('open');
                
                const event = new Event('change', { bubbles: true });
                select.dispatchEvent(event);
            });
            
            optionsContainer.appendChild(customOption);
        });
        
        trigger.addEventListener('click', (e) => {
            e.stopPropagation();
            
            document.querySelectorAll('.custom-select-wrapper.open').forEach(w => {
                if (w !== wrapper) {
                    w.classList.remove('open');
                }
            });
            
            wrapper.classList.toggle('open');
        });
        
        select.parentNode.insertBefore(wrapper, select);
        wrapper.appendChild(trigger);
        wrapper.appendChild(optionsContainer);
        wrapper.appendChild(select);
    });
    
    document.addEventListener('click', () => {
        document.querySelectorAll('.custom-select-wrapper.open').forEach(w => {
            w.classList.remove('open');
        });
    });
}

function updateCustomSelect(selectElement) {
    const wrapper = selectElement.closest('.custom-select-wrapper');
    if (!wrapper) return;
    
    const trigger = wrapper.querySelector('.custom-select-trigger');
    const optionsContainer = wrapper.querySelector('.custom-select-options');
    
    optionsContainer.innerHTML = '';
    
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
    
    const selectedOption = selectElement.options[selectElement.selectedIndex];
    if (selectedOption) {
        trigger.textContent = selectedOption.textContent;
    }
}

function syncCustomSelectValue(selectElement) {
    const wrapper = selectElement.closest('.custom-select-wrapper');
    if (!wrapper) return;
    
    const trigger = wrapper.querySelector('.custom-select-trigger');
    const optionsContainer = wrapper.querySelector('.custom-select-options');
    
    const selectedOption = selectElement.options[selectElement.selectedIndex];
    if (selectedOption) {
        trigger.textContent = selectedOption.textContent;
    }
    
    optionsContainer.querySelectorAll('.custom-select-option').forEach((opt, index) => {
        if (index === selectElement.selectedIndex) {
            opt.classList.add('selected');
        } else {
            opt.classList.remove('selected');
        }
    });
}

function updateCustomSelectsText() {
    document.querySelectorAll('.custom-select-wrapper').forEach(wrapper => {
        const select = wrapper.querySelector('select');
        const trigger = wrapper.querySelector('.custom-select-trigger');
        const optionsContainer = wrapper.querySelector('.custom-select-options');
        
        if (!select || !trigger || !optionsContainer) return;
        
        const selectedOption = select.options[select.selectedIndex];
        if (selectedOption) {
            trigger.textContent = selectedOption.textContent;
        }
        
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

document.addEventListener('wheel', function (e) {
    if (e.ctrlKey) {
        e.preventDefault();
    }
}, {passive: false});
