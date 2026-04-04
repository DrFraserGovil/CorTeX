#define JSL_INCLUDE_LOG
#include "JSL/JSL.h"
Initialise_JSL_Log()
#include <iostream>

#include "settings/validate.h"
#include "interface/initialiser.h"
#include "interface/interface.h"
#include "async/watcher.h"
#include "async/worker.h"
#include "files/directory.h"
#include "project/index.h"
FileIndex MasterIndex;
int main(int argc, char**argv)
{
    Initialiser Bootstrap(argc,argv);
    Interface Menu;
    
    //now use the setup we have to initialise the vital structures
    
    ValidateSettings();    
    Worker Manager;
    MasterIndex.Initialise();
    SystemWatcher Watcher(Manager);
    
    if (!Menu.Headless)
    {
        Menu.ConnectWorker(Manager);
        auto workthread = std::thread(&Worker::WorkerLoop,&Manager,std::ref(Watcher)); //launch the asynchronous worker 

        
        Menu.BeginLoop();

        if (workthread.joinable())
        {
            workthread.join();
        }
    }
    else
    {
        LOG(INFO) << "Entering headless mode";
        Manager.WorkerLoop(Watcher);
    }
    

    MasterIndex.UnwatchAll();
    LOG(INFO) << "CorTeX Shutdown complete";
}
