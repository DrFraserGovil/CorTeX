#define JSL_INCLUDE_LOG
#include "JSL/JSL.h"
Initialise_JSL_Log()
#include <iostream>

#include "settings/validate.h"
#include "interface/initialiser.h"
#include "interface/interface.h"
#include "async/watcher.h"
#include "async/worker.h"

int main(int argc, char**argv)
{
    Initialiser Bootstrap(argc,argv);
    Interface Menu(Bootstrap.GetMetadata());
    ValidateSettings();    

    Worker Manager;
    SystemWatcher Watcher(Settings.Files.TargetDirectory,Manager);
    
    Menu.ConnectWorker(Manager);
    Watcher.GetWatchedDirs();
    
    if (!Settings.System.Headless.Active)
    {
        auto workthread = std::thread(&Worker::WorkerLoop,&Manager,std::ref(Watcher)); //launch the asynchronous worker (also spawns a second threead internally)
        
        // Manager.WaitForInitialisation();
        Menu.BeginLoop();

        if (workthread.joinable())
        {
            workthread.join();
        }
    }
    LOG(INFO) << "CorTeX Shutdown complete";
    // auto Foam = FoamTexArchive();
   
    // auto q = SystemWatcher(".");

    // LOG(INFO) << "Foamtex Sentinel Activating...";
    // q.Listen();
    // LOG(INFO) << "Foamtex exiting";
}
