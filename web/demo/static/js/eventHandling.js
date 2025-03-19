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

function eventHandling() {
    const pageContent = document.querySelector('.page-content');
    const mobileWarning = document.getElementById('mobile-warning');
    const browserWarning = document.getElementById('browser-warning');

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

    if (!isDesktop()) {
        pageContent.classList.add('hidden');
        mobileWarning.classList.remove('hidden');
        setTimeout(() => {
            mobileWarning.classList.add('show');
        }, 100);
        return false;
    }

    if (!isChromiumBased()) {
        pageContent.classList.add('hidden');
        browserWarning.classList.remove('hidden');
        setTimeout(() => {
            browserWarning.classList.add('show');
        }, 100);
        return false;
    }

    return true;
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

function updatePerfInfo(fps, drawTime, drawCount) {
    const fpsElement = document.querySelector('.fps');
    const timeElement = document.querySelector('.time');
    const countElement = document.querySelector('.count');
    if (fpsElement) fpsElement.textContent = Number(fps).toFixed(1);
    if (timeElement) timeElement.textContent = Number(drawTime).toFixed(1);
    if (countElement) countElement.textContent = drawCount;
}

window.updatePerfInfo = updatePerfInfo;
document.addEventListener('DOMContentLoaded', eventHandling);
document.addEventListener('DOMContentLoaded', initLanguage);
document.addEventListener('DOMContentLoaded', function () {
    const languageSelect = document.getElementById('language-type');
    languageSelect.addEventListener('change', function () {
        const selectedLanguage = this.value;
        console.log(`selectedLanguage:${selectedLanguage}`);
        handleLanguageChange(selectedLanguage);
    });
});