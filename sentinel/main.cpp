#define JSL_INCLUDE_LOG
#include "JSL/JSL.h"
Initialise_JSL_Log()
#include <iostream>

#include "settings/settings.hpp"
#include "interface/initialiser.h"
#include "interface/interface.h"
#include "async/watcher.h"
#include "async/worker.h"

int main(int argc, char**argv)
{
    Initialiser Bootstrap(argc,argv);
    Interface Menu(Bootstrap.GetMetadata());

    Worker W;
    Menu.ConnectWorker(W);
    
    SystemWatcher Watch(Settings.Files.TargetDirectory);
    Watch.GetWatchedDirs();
    if (!Settings.System.Headless.Active)
    {
        auto workthread = std::thread(&Worker::WorkerLoop,&W,std::ref(Watch)); //launch the asynchronous worker
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
