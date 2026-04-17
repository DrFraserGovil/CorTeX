import * as vscode from 'vscode';
import { CortexEngine } from './core/CortexEngine';
import { CustomProvider } from './viewer/CustomProvider';
import { ViewManager } from './viewer/ViewManager';
import { CortexFileProvider } from './viewer/FileExplorer';
export function activate(context: vscode.ExtensionContext): void
{
	const launchCommand = vscode.commands.registerCommand('cortex.launch', () => {
        vscode.window.showInformationMessage('Cortex is launching...');
    });
	vscode.window.showInformationMessage('Cortex is launching...');
	
	//metadata & link engine
	const extensionRoot = context.extensionUri;

	const engine = new CortexEngine();
	const manager = new ViewManager(engine,extensionRoot);
	const provider = new CustomProvider(manager);

	const vsRegister=  vscode.window.registerCustomEditorProvider(
      CustomProvider.viewType,
      provider,
      {
        webviewOptions: {
          enableFindWidget: false, // default
          retainContextWhenHidden: true,
        },
      }
    );

	context.subscriptions.push(engine,manager,vsRegister);
	
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
    });
	
	context.subscriptions.push(rebootCommand,pauseCommand,resumeCommand);
	const root = vscode.workspace.workspaceFolders;;
	if (root)
	{
		const fileProvider = new CortexFileProvider(root[0].uri.fsPath);
		vscode.window.registerTreeDataProvider('cortex.files', fileProvider);

		context.subscriptions.push(vscode.commands.registerCommand('cortex.openFile', async (uri: vscode.Uri) => {
			await manager.flash(uri);
		}));
	}

	


}

export function deactivate()
{
	
}