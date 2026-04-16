import * as vscode from 'vscode';
import { ViewManager } from './ViewManager';
export class CustomProvider implements vscode.CustomReadonlyEditorProvider 
{
    public static readonly viewType = 'cortex.preview';

   constructor(private readonly manager: ViewManager) {}

    public async resolveCustomEditor(document: vscode.CustomDocument, webviewPanel: vscode.WebviewPanel)
    {
        // // The Manager handles everything from here
        // const activeTab = this.manager.activeTab();
        

        // if (this.manager.canHotSwap(document.uri))
        // {
        //     await this.manager.open(document.uri, webviewPanel);
        //     setImmediate(async () => {
        //     await vscode.commands.executeCommand('workbench.action.closeActiveEditor');
        // });
         
            
        // return;
        await this.manager.open(document.uri, webviewPanel);
    }


    public openCustomDocument(uri: vscode.Uri): vscode.CustomDocument 
    {
        return { uri, dispose: () => {} };
    }
}