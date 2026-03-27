#pragma once
#include "globalSettings.h"
#include "watcher.h"
#include "compiled.h"
#include "sourceHandler.h"
#include <condition_variable>

class FoamTexArchive
{   
    public:
        FoamTexArchive();
        
    private:
        SystemWatcher Watcher;
        SourceHandler SourceCode;
        CompiledOutput Compiled;

        void WatcherLoop();

        void ProcessBuffer( std::map<fs::path,FileReport> batch);

        void DirectoryUpdate(FileReport report);
};