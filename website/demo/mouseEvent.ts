export function setupCoordinateConversion(canvasId: string, showSideBar: boolean) {
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