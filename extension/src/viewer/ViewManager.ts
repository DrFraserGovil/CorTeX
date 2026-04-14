import * as vscode from 'vscode';
import { CortexEngine } from '../core/CortexEngine';
import { ViewerTab } from './ViewerTab';
import { CortexPreview } from './CortexPreview';
export class ViewManager
{
    private readonly _tabs = new Map<string,ViewerTab>
    private readonly _previews = new Map<string,CortexPreview>

    constructor(
        private readonly engine: CortexEngine,
        private readonly extensionUri: vscode.Uri
    ) {}

    /**
     * Called by the Provider whenever a PDF file is opened
     */
    public async open(uri: vscode.Uri, panel: vscode.WebviewPanel)
    {
        const resourceId = uri.toString();
        
        // Create the preview instance
        const preview = new ViewerTab(this.extensionUri, uri, panel,this);
        this._tabs.set(resourceId, preview);

        // Clean up when closed
        panel.onDidDispose(() =>
        {
            this._tabs.delete(resourceId);
        });
    }

    public async handleLinkNavigation(sourceUri: vscode.Uri, targetPath: string)
    {
        //placeholder
        console.log("Got",sourceUri,targetPath);
    }
}