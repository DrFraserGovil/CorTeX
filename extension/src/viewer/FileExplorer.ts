import * as vscode from 'vscode';
import * as path from 'path';

export class CortexFileProvider implements vscode.TreeDataProvider<CortexFile>
{
    constructor(private workspaceRoot: string | undefined) {
        this.setWatchers();
    }

    getTreeItem(element: CortexFile): vscode.TreeItem
    {
        return element;
    }

    private _onDidChangeTreeData: vscode.EventEmitter<CortexFile | undefined | null | void> = 
    new vscode.EventEmitter<CortexFile | undefined | null | void>();

// 2. This is the public 'event' the TreeView subscribes to
    readonly onDidChangeTreeData: vscode.Event<CortexFile | undefined | null | void> = 
        this._onDidChangeTreeData.event;
    public refresh(): void {
        this._onDidChangeTreeData.fire();
    }

    private setWatchers(): void {
        if (!this.workspaceRoot) return;

        // Watch for PDF creation, changes, or deletions
        // We use a relative pattern to keep it efficient
        const watcher = vscode.workspace.createFileSystemWatcher(
            new vscode.RelativePattern(this.workspaceRoot, '**/*.pdf')
        );

        // Refresh the sidebar whenever a PDF is added, changed, or deleted
        watcher.onDidCreate(() => this.refresh());
        watcher.onDidChange(() => this.refresh());
        watcher.onDidDelete(() => this.refresh());
    }

    async getChildren(element?: CortexFile): Promise<CortexFile[]>
    {
        if (!this.workspaceRoot) return [];

        // Simple filtering logic
        const dir = element ? element.resourceUri.fsPath : this.workspaceRoot;
        const entries = await vscode.workspace.fs.readDirectory(vscode.Uri.file(dir));

        return entries
            .filter(([name, type]) => {
                // 1. FILTER: Ignore common build directories
                if (['build', 'out', 'bin', 'node_modules'].includes(name)) return false;
                
                // 2. FILTER: Only show directories or PDF files
                return type === vscode.FileType.Directory || name.endsWith('.pdf');
            })
            .map(([name, type]) => {
                const uri = vscode.Uri.file(path.join(dir, name));
                return new CortexFile(
                    name, 
                    type === vscode.FileType.Directory 
                        ? vscode.TreeItemCollapsibleState.Collapsed 
                        : vscode.TreeItemCollapsibleState.None,
                    uri
                );
            });
    }
}

class CortexFile extends vscode.TreeItem {
    constructor(
        public readonly label: string,
        public readonly collapsibleState: vscode.TreeItemCollapsibleState,
        public readonly resourceUri: vscode.Uri
    ) {
        super(label, collapsibleState);
        
        // This is the secret sauce: 
        // A left-click triggers our command, NOT the default editor open.
        if (this.collapsibleState === vscode.TreeItemCollapsibleState.None) {
            this.command = {
                command: 'cortex.openFile',
                title: 'Open File',
                arguments: [this.resourceUri]
            };
            this.contextValue = 'pdf';
            // console.log("Register",resourceUri.fsPath);
        }
    }
}