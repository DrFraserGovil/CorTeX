import * as vscode from 'vscode';
import { CortexEngine } from './core/CortexEngine';

export function activate(context: vscode.ExtensionContext): void
{
	const launchCommand = vscode.commands.registerCommand('cortex.launch', () => {
        vscode.window.showInformationMessage('Cortex is launching...');
    });
	vscode.window.showInformationMessage('Cortex is launching...');
	
	//metadata & link engine
	const engine = new CortexEngine();
	context.subscriptions.push(engine);
	
	
	const rebootCommand = vscode.commands.registerCommand('cortex.reboot', () => {
		vscode.window.showInformationMessage('Cortex is relaunching...');
		engine.bootCortex();
    });
	const pauseCommand = vscode.commands.registerCommand('cortex.pause', () => {
		vscode.window.showInformationMessage('Cortex Compilation Is Paused');
		engine.EngineCommand("pause");
    });
	const resumeCommand = vscode.commands.registerCommand('cortex.resume', () => {
		engine.EngineCommand("resume");
		engine.bootCortex();
    });
	
}

export function deactivate()
{
	
}