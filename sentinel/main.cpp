#define JSL_INCLUDE_LOG
#include "JSL/JSL.h"
Initialise_JSL_Log()
#include <iostream>
#include "global.h"
#include "project/project.h"
#include "interface/watcher.h"
#include "interface/worker.h"
void HeadlessMain()
{

};


void InterfaceMain()
{
    Watcher watcher;
    Worker worker;

    watcher.Connect(&worker);
    
    worker.ProcessInput(); //main loop which waits for an exit signal

    watcher.Exit();
};



Project Cortex;

int main(int argc, char**argv)
{
    Cortex.Initialise(argc,argv);
    if (Settings.System.Headless.Active)
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
