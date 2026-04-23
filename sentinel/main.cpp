#define JSL_INCLUDE_LOG
#include "JSL.h"
Initialise_JSL_Log()
#include <iostream>
#include <csignal>
#include "global.h"
#include "project/project.h"
#include "async/watcher.h"
#include "async/worker.h"




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
    // setvbuf(stdout, NULL, _IONBF, 0); //uncomment this line to disable output buffering, which can cause logs to not appear until the program has finished (e.g. after a crash)

    std::signal(SIGINT, InterruptShutdown);
    auto type = Cortex.Initialise(argc,argv);
 
    if (type == Mode::Interactive)
    {
        Cortex.BeginInterface();
    }
    else if (type == Mode::SingleCommand)
    {
        Cortex.SingleCommand();
    }
    else
    {
        LOG(ERROR) << "Invalid mode detected, shutting down";
    }


    Cortex.Shutdown();
}