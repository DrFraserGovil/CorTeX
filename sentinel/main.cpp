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

int main(int argc, char**argv)
{
    Initialiser Bootstrap(argc,argv);
    Interface Menu(Bootstrap.GetMetadata());
    ValidateSettings();    

    Worker Manager;
    auto Root = Directory::GetRoot();
    SystemWatcher Watcher(Root,Manager);
    Menu.ConnectWorker(Manager);
    
    if (!Settings.System.Headless.Active)
    {
        auto workthread = std::thread(&Worker::WorkerLoop,&Manager,std::ref(Watcher)); //launch the asynchronous worker (also spawns a second threead internally)
        
        Menu.BeginLoop();

        if (workthread.joinable())
        {
            workthread.join();
        }
    }
    else
    {
        LOG(ERROR) << "Headless mode not yet implemented";
    //    auto Root = Directory::GetRoot();
    }

    Root->Unwatch();
    LOG(INFO) << "CorTeX Shutdown complete";
}
