#define JSL_INCLUDE_LOG
#include "JSL/JSL.h"
Initialise_JSL_Log()
#include <iostream>
#include <csignal>
#include "global.h"
#include "project/project.h"
#include "async/watcher.h"
#include "async/worker.h"


void InterfaceMain()
{
    WatcherObject watcher;
    WorkerObject worker;

    Cortex.Connect(&worker,&watcher);
    
    worker.ProcessInput(); //main loop which waits for an exit signal

    watcher.Exit();
};


void InterruptShutdown(int signal)
{
    if (signal == SIGINT)
    {
        LOG(ERROR) << "SIGINT signal recieved, attempting graceful shutdown";

        Cortex.Worker->AddTask(Instruction::Shutdown);
    }
}


Project Cortex;

int main(int argc, char**argv)
{
    Cortex.Initialise(argc,argv);
 
    std::signal(SIGINT, InterruptShutdown);

    InterfaceMain();

    Cortex.Shutdown();
}