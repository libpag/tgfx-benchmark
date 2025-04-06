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
    return resourceBundle;
}

function updatePerfInfo(fps, drawTime, drawCount, maxDrawCountReached) {
    const fpsElement = document.querySelector('.fps');
    const fpsParagraph = document.querySelector('.fpsP');
    const timeElement = document.querySelector('.time');
    const timeParagraph = document.querySelector('.timeP');
    const countElement = document.querySelector('.count');
    if (fpsElement && fpsParagraph) {
        fpsElement.textContent = Number(fps).toFixed(1);
        const color = fps < 29 ? '#FF4444' : fps < 59 ? '#FFAA00' : '#4CAF50';
        fpsElement.style.color = color;
        fpsParagraph.style.color = color;
    }
    if (timeElement && timeParagraph) {
        timeElement.textContent = Number(drawTime).toFixed(1);
        const color = drawTime > 34.3 ? '#FF4444' : drawTime > 16.9 ? '#FFAA00' : '#4CAF50';
        timeElement.style.color = color;
        timeParagraph.style.color = color;
    }
    const targetFPS = parseFloat(document.getElementById('minFPS').value) || 60;

    maxDrawCountReached = !!maxDrawCountReached;
    if (maxDrawCountReached) {
        if (countElement) countElement.textContent = `[${drawCount}]`;
    } else {
        if (countElement) countElement.textContent = drawCount;
    }
}

window.updatePerfInfo = updatePerfInfo;
document.addEventListener('DOMContentLoaded', checkBrowser);
document.addEventListener('DOMContentLoaded', initLanguage);
document.addEventListener('DOMContentLoaded', function () {
    const languageSelect = document.getElementById('language-type');
    languageSelect.addEventListener('change', function () {
        const selectedLanguage = this.value;
        handleLanguageChange(selectedLanguage);
    });
});

document.addEventListener('wheel', function(e) {
if (e.ctrlKey) {
    e.preventDefault();
}
}, { passive: false });
