#define JSL_INCLUDE_LOG
#include "JSL/JSL.h"
Initialise_JSL_Log()
#include <iostream>

#include "settings/settings.hpp"
#include "interface/initialiser.h"
#include "interface/interface.h"

int main(int argc, char**argv)
{
    Initialiser Bootstrap(argc,argv);
    Interface Menu(Bootstrap.GetMetadata());


    // auto Foam = FoamTexArchive();
   
    // auto q = SystemWatcher(".");

    // LOG(INFO) << "Foamtex Sentinel Activating...";
    // q.Listen();
    // LOG(INFO) << "Foamtex exiting";
}
