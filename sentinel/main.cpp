#define JSL_INCLUDE_LOG
#include "JSL/JSL.h"
Initialise_JSL_Log()
#include <iostream>
#include "global.h"
#include "project/project.h"
#include "async/watcher.h"
#include "async/worker.h"
void HeadlessMain()
{

};


void InterfaceMain()
{
    WatcherObject watcher;
    WorkerObject worker;

    Cortex.Connect(&worker,&watcher);
    
    worker.ProcessInput(); //main loop which waits for an exit signal

    watcher.Exit();
};



Project Cortex;

int main(int argc, char**argv)
{
    Cortex.Initialise(argc,argv);
    if (Cortex.Settings.System.Headless.Active)
    {
        HeadlessMain();
    }
    else
    {
        InterfaceMain();
    }

    LOG(INFO) << "CorTeX Shutdown complete";
}


// FileIndex MasterIndex;
//     MasterIndex.Initialise();


//     MasterIndex.UnwatchAll();
//     LOG(INFO) << "CorTeX Shutdown complete";
// }
