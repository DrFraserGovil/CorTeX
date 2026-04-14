import * as vscode from 'vscode';
import * as path from 'path';
import { ViewManager } from './ViewManager';
export class ViewerTab
{
    private resource: vscode.Uri;
    constructor(
        private readonly extensionRoot: vscode.Uri,
        initialResource: vscode.Uri,
        private readonly panel: vscode.WebviewPanel,
        private readonly manager: ViewManager
    ) {
        this.resource = initialResource;
        this.setupWebview();
    }

    private setupWebview() 
    {
        this.panel.webview.options = {
            enableScripts: true,
            localResourceRoots: [
                vscode.Uri.file(path.join(this.extensionRoot.fsPath, 'media')),
                vscode.Uri.file(path.dirname(this.resource.fsPath))
            ]
        };

        this.panel.webview.html = this.getHtml();

        this.panel.webview.onDidReceiveMessage(async (msg) => 
        {
            if (msg.type === 'did-click-link') 
            {
                const targetUri = vscode.Uri.file(path.resolve(path.dirname(this.resource.fsPath), msg.uri));
                this.manager.handleLinkNavigation(this.resource,targetUri.toString());
            }
        });
    }

    private hotswap(newUri: vscode.Uri) 
    {
        this.resource = newUri;
        this.panel.title = path.basename(newUri.fsPath);
        
        // Tell the JS to just load a new blob without reloading the whole iframe
        this.panel.webview.postMessage({
            type: 'load-new-pdf',
            path: this.panel.webview.asWebviewUri(newUri).toString()
        });
    }

    private getHtml() 
    {
        const viewerPath = this.panel.webview.asWebviewUri(
            vscode.Uri.file(path.join(this.extensionRoot.fsPath, 'media', 'web', 'viewer.html'))
        );
        
        // We embed the PDF.js viewer.html as an iframe so we can control it via main.js
        return `
            <!DOCTYPE html>
            <html>
            <body style="margin:0;padding:0;width:100%;height:100vh;overflow:hidden;">
                <iframe 
                    id="viewer" 
                    src="${viewerPath}?file=${encodeURIComponent(this.panel.webview.asWebviewUri(this.resource).toString())}" 
                    style="width:100%;height:100%;border:none;">
                </iframe>
                <script src="${this.panel.webview.asWebviewUri(vscode.Uri.file(path.join(this.extensionRoot.fsPath, 'media', 'main.js')))}"></script>
            </body>
            </html>`;
    }
};