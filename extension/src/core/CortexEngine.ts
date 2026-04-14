import * as vscode from 'vscode';
import { spawn, ChildProcess } from 'child_process';
import { Disposable } from '../utils/disposable';

//central engine and manager of the extension processes

export class CortexEngine extends Disposable
{
    private engineHost?: ChildProcess;
    private messageHost?: ChildProcess;
    
    constructor(){
        super();
        console.log("CorTeX Metadata Layer Initialised");
        this.bootCortex();
    }

    private bootCortex()
    {
        const projectDirectory = vscode.workspace.workspaceFolders?.[0].uri.fsPath;
        console.log("Launching cortex in",projectDirectory);
        if (projectDirectory)
        {
            this.engineHost = spawn("/home/jfg/CodeProjects/cortex/cortex",['-headless','-i',projectDirectory],
            {
                shell: false,
                stdio: ['pipe','pipe','pipe'], //in, out and err,
                env: process.env
            });

            this.engineHost.on('error', (err) => 
            {
                vscode.window.showErrorMessage(`Cortex Engine Error: ${err.message}`);
            });
            this.engineHost.stderr?.on('data', (data) => {
                console.error(`${data.toString()}`);
            });

            this.engineHost.stdout?.on('data', (data) => {
                console.log(`${data.toString()}`);
            });

            // Ensure that if the process dies unexpectedly, we know about it
            this.engineHost.on('exit', (code) => 
            {
                if (code == 2)
                {
                    console.log("An external cortex process is already running in this directory: communication established");
                    this.engineHost = undefined;
                }
                else
                {
                    console.log(`Cortex Engine exited with code ${code}`);
                }
            });
        }
        else
        {
            console.error("Could not launch cortex process");
        }

       
    }

    public override dispose() 
    {
        this.shutdown();
        super.dispose();
    }

    private shutdown()
    {
        const host = this.engineHost;
        if (host)
        {
            this.messageHost = spawn("/home/jfg/CodeProjects/cortex/cortex",['shutdown']);

            //or kill it 
            setTimeout(() =>
            {
                host.kill();
            }, 1000);
        }
    }
}
