import * as vscode from 'vscode';
import { CortexEngine } from './core/CortexEngine';

export function activate(context: vscode.ExtensionContext): void
{
	vscode.window.showInformationMessage('Initialising CorTeX');

	//metadata & link engine
	const engine = new CortexEngine();
	context.subscriptions.push(engine);


}

export function deactivate()
{
	
}