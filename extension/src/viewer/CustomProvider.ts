import * as vscode from 'vscode';
import { ViewManager } from './ViewManager';
export class PdfCustomProvider implements vscode.CustomReadonlyEditorProvider 
{
    public static readonly viewType = 'cortex.pdfViewer';

   constructor(private readonly manager: ViewManager) {}

    public async resolveCustomEditor(document: vscode.CustomDocument, webviewPanel: vscode.WebviewPanel)
    {
        // The Manager handles everything from here
        await this.manager.open(document.uri, webviewPanel);
    }


    public openCustomDocument(uri: vscode.Uri): vscode.CustomDocument 
    {
        return { uri, dispose: () => {} };
    }
}