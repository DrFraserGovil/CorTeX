"use strict";

(function () {
    const vscode = acquireVsCodeApi();

    // --- Helper Functions ---

    function loadConfig() {
        const elem = document.getElementById('pdf-preview-config');
        if (elem) return JSON.parse(elem.getAttribute('data-config'));
        throw new Error('Could not load configuration.');
    }

    function cursorTools(name) {
        return name === 'hand' ? 1 : 0;
    }

    function scrollMode(name) {
        const modes = { 'vertical': 0, 'horizontal': 1, 'wrapped': 2 };
        return modes[name] ?? -1;
    }

    function spreadMode(name) {
        const modes = { 'none': 0, 'odd': 1, 'even': 2 };
        return modes[name] ?? -1;
    }

    // --- Lifecycle and Initialization ---

    window.addEventListener('load', async function () {
        const config = loadConfig();
        const loadOpts = {
            url: config.path,
            useWorkerFetch: false,
            cMapUrl: config.cMapUrl,
            cMapPacked: true,
            standardFontDataUrl: config.standardFontDataUrl
        };

        PDFViewerApplication.initializedPromise.then(() => {
            const defaults = config.defaults;

            const optsOnLoad = () => {
                PDFViewerApplication.pdfCursorTools.switchTool(cursorTools(defaults.cursor));
                PDFViewerApplication.pdfViewer.scrollMode = scrollMode(defaults.scrollMode);
                PDFViewerApplication.pdfViewer.spreadMode = spreadMode(defaults.spreadMode);
                PDFViewerApplication.pdfViewer.currentScaleValue = defaults.scale;
                PDFViewerApplicationOptions.set('defaultZoomValue', defaults.scale);
                console.log("ZOOM:",PDFViewerApplication.pdfViewer.currentScaleValue,defaults.scale);
                if (defaults.sidebar) {
                    PDFViewerApplication.pdfSidebar.open();
                } else {
                    PDFViewerApplication.pdfSidebar.close();
                }
                PDFViewerApplication.eventBus.off('documentloaded', optsOnLoad);
            };

            PDFViewerApplication.eventBus.on('documentloaded', optsOnLoad);

            // 3. Open Document & Prime initial cache
            PDFViewerApplication.open(config.path).then(async function () {
                const doc = await pdfjsLib.getDocument(loadOpts).promise;
                doc._pdfInfo.fingerprints = [config.path];
                
                
                PDFViewerApplication.load(doc);
            });

    //         // Create the Back Button
    //         const backBtn = document.createElement('button');
    //         backBtn.id = 'customBack';
    //         backBtn.className = 'toolbarButton';
    //         backBtn.title = 'Go Back';
    //         backBtn.style.fontWeight = 'bold';
    //         backBtn.style.marginRight = '10px';
    //         backBtn.innerHTML = `
    //     <span class="back-icon"></span>
    //     <span class="label">Back</span>z`
    // `;

    //         // Inject it into the primary toolbar (before the page number)
    //         const toolbar = document.getElementById('toolbarViewerLeft');
    //         const numPages = document.getElementById('numPages');
    //         if (toolbar && numPages) {
    //             toolbar.insertBefore(backBtn, toolbar.firstChild);
    //         }

    //         backBtn.addEventListener('click', () => {
    //             vscode.postMessage({ type: 'request-back' });
    //         });
        });
    }, { once: true });

    // --- Interceptor ---


    
    document.addEventListener('mousedown', (event) => {
        const section = event.target.closest('.linkAnnotation');
        if (event.button != 0 && event.button != 2){return;}
        const leftClick = (event.button == 0) && !(event.metaKey || event.ctrlKey);
        if (section)
        {
            const target = section.getAttribute('data-annotation-id');
            if (target)
            {
                event.preventDefault(); // prevent default behaviour
    
                vscode.postMessage({
                    type: 'did-click-link',
                    contents: target,
                    leftClick: leftClick
                })
            }
        }
    },true);


    //     // Only intercept Left Click
    //     // const normalClick = (event.button==0) && !(event.metaKey || event.ctrlKey) 
    //     // console.log(event.button,event.metaKey,normalClick)
    //     // if (!section) return;

    //     // const annotationId = section.getAttribute('data-annotation-id');
    //     // const pageNumber = PDFViewerApplication.page;
    //     // const annotations = annotationCache.get(pageNumber);

    //     // if (!annotations) return;

    //     // const data = annotations.find(a => a.id === annotationId);
    //     // if (!data) return;

    //     // // Extract URI
    //     // const rawUri = data.url || data.unsafeUrl || data.file;
    //     // if (rawUri) {
    //     //     event.preventDefault();
    //     //     event.stopPropagation();

    //     //     vscode.postMessage({
    //     //         type: 'did-click-link',
    //     //         uri: rawUri.split('#')[0],
    //     //           leftClick: normalClick 
    //     //     });
    //     // }
    // }, true);


    // main.js

async function initializeAnnotationListener() {
    // 1. Wait for the application to be ready
    if (!window.PDFViewerApplication || !window.PDFViewerApplication.initializedPromise) {
        console.warn("Cortex: Viewer not ready yet, retrying...");
        setTimeout(initializeAnnotationListener, 50);
        return;
    }

    await PDFViewerApplication.initializedPromise;

    const eventBus = PDFViewerApplication.eventBus;
    if (!eventBus) {
        console.error("Cortex: EventBus not found!");
        return;
    }

    // 2. Now it's safe to attach listeners
    eventBus.on('annotationlayerrendered', (ev) => {
        // Use the event detail directly to get the page
        const pageView = ev.source; 
        if (pageView && pageView.pdfPage) {
            const annt = pageView.pdfPage.getAnnotations();
            console.log(annt);
            pageView.pdfPage.getAnnotations().then(annotations => {
                vscode.postMessage({
                    type: 'annotation-data',
                    annotations: annotations.map(a => ({
                        id: a.id,
                        url: a.url,
                        unsafeUrl: a.unsafeUrl,
                        file: a.file
                    }))
                });
            });
        }
    });
}

// Kick off the initialization
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeAnnotationListener);
} else {
    initializeAnnotationListener();
}
    
    window.onerror = function () {
        const msg = document.createElement('body');
        msg.innerText = 'An error occurred while loading the file.';
        document.body = msg;
    };
}());
