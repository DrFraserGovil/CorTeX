import * as vscode from 'vscode';
import { CortexEngine } from './core/CortexEngine';

export function activate(context: vscode.ExtensionContext): void
{
	const launchCommand = vscode.commands.registerCommand('cortex.launch', () => {
        vscode.window.showInformationMessage('Cortex is launching...');
    });
	const rebootCommand = vscode.commands.registerCommand('cortex.reboot', () => {
        vscode.window.showInformationMessage('Cortex is launching...');
    });


	//metadata & link engine
	const engine = new CortexEngine();
	context.subscriptions.push(engine);


}

export function deactivate()
{
	
}