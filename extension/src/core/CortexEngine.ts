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
        this.engineHost = spawn("/home/jfg/CodeProjects/cortex/cortex",['-headless'],
        {
            shell: true,
            stdio: ['pipe','pipe','pipe'], //in, out and err,
            env: process.env
        });

        this.engineHost.on('error', (err) => 
            {
                vscode.window.showErrorMessage(`Cortex Engine Error: ${err.message}`);
            });
            this.engineHost.stderr?.on('data', (data) => {
    console.error(`Cortex Native Error: ${data.toString()}`);
});

this.engineHost.stdout?.on('data', (data) => {
    console.log(`Cortex Native Output: ${data.toString()}`);
});

        // Ensure that if the process dies unexpectedly, we know about it
        this.engineHost.on('exit', (code) => 
            {
                console.log(`Cortex Engine exited with code ${code}`);
            });
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
