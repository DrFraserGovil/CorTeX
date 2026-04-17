import * as vscode from 'vscode';
import { CortexEngine } from '../core/CortexEngine';
import { ViewerTab } from './ViewerTab';
import { CortexPreview } from './CortexPreview';
import { Disposable } from '../utils/disposable';

async function fileExists(uri: vscode.Uri): Promise<boolean> {
	try {
		await vscode.workspace.fs.stat(uri);
		return true;
	} catch {
		return false;
	}
}


export class ViewManager extends Disposable
{
	private readonly _tabs = new Map<number, ViewerTab>;
	private readonly _tabRegistry = new Map<string,number>;
	private readonly _previews = new Map<string,CortexPreview>
	private sequentialID = 0;
	constructor(
		private readonly engine: CortexEngine,
		private readonly extensionUri: vscode.Uri
	) {
		super();
		console.log("View Manager Initialised");
	}

	private getPreview(uri : vscode.Uri) : CortexPreview
	{
		const id = uri.toString();
		const tmp = this._previews.get(id);
		if (tmp)
		{
			return tmp;
		}
		else
		{
			const tmp = new CortexPreview(uri)
			this._previews.set(id,tmp);
			
			return tmp;
		}
	}

	/**
	 * Called by the Provider whenever a PDF file is opened
	 */
	public async open(uri: vscode.Uri, panel: vscode.WebviewPanel)
	{
		// console.log("New open called",this._tabs.size);
		const activeTab = this.getActiveTab();
		const file = this.getPreview(uri);
		const name= uri.toString();
	
		
		console.log("Creating a new tab with id", this.sequentialID);
		const tab = new ViewerTab(this.extensionUri, file, panel,this,this.sequentialID);
		this._tabs.set(this.sequentialID,tab);
		this._tabRegistry.set(name,this.sequentialID);

		
		// Clean up when closed
		panel.onDidDispose(() =>
		{
			const tabInstance = this._tabs.get(tab.ID);
			if (tabInstance)
			{
				const oldName = tabInstance.GetName();
				if (oldName)
				{
					this._tabRegistry.delete(oldName);
				}
			}
			this._tabs.delete(tab.ID);
		});
		this.sequentialID++;
		
	}

	public async flash(uri: vscode.Uri)
	{
		const activeTab = this.getActiveTab();
		if (!activeTab)
		{
			await vscode.commands.executeCommand('vscode.open', uri, {
       			viewColumn: vscode.ViewColumn.Active,
        		preserveFocus: false,
    		});
			return;
		}
		const file = this.getPreview(uri);
		if (file)
		{
			activeTab.show(file);
		}
	}

	public getActiveTab() : ViewerTab | undefined
	{
		return (this._tabs.size > 0) ? Array.from(this._tabs.values()).find(tab => tab.isActive()) : undefined;
	}


	public async handleLinkNavigation(callingTab: ViewerTab, to: string, leftClick: boolean | undefined)
	{

		const fromFile = callingTab.activeFile.Source;
		//placeholder

		const toUri = vscode.Uri.joinPath(fromFile,'..',to);

		//check file exists
		if (! await fileExists(toUri))
		{
			vscode.window.showErrorMessage(`File '${to}' cannot be accessed`);
			return;
		}

		if (toUri)
		{            
			
			
			// attempt to retrieve from open tabs
			const id = this._tabRegistry.get(toUri.toString());
			// If id is undefined, tab will also be undefined automatically
			const existingTab = (id !== undefined) ? this._tabs.get(id) : undefined;

			if (existingTab)
			{
				//a tab exists with this file open, so we just context switch to it
				existingTab.Focus();
				return;
			}

			console.log(leftClick);

			if (leftClick)
			{
				//otherwise we should hotswap the current tab to the new file
				callingTab.show(this.getPreview(toUri));
			}
			else
			{
				await vscode.commands.executeCommand('vscode.open', toUri, {
										viewColumn: vscode.ViewColumn.Active,
										preview: true 
									});
			}

		}
	}
}