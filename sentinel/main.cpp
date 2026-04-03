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
#include "files/index.h"
FileIndex MasterIndex;

int main(int argc, char**argv)
{
    Initialiser Bootstrap(argc,argv);
    Interface Menu(Bootstrap.GetMetadata());
    ValidateSettings();    

    Worker Manager;
    auto Root = Directory::GetRoot();
    SystemWatcher Watcher(Root,Manager);
    
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
    

    Root->Unwatch();
    LOG(INFO) << "CorTeX Shutdown complete";
}
