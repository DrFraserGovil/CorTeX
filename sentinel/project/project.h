#pragma once

#include "metadata.h"
#include "../index/fileindex.h"
#include "../settings/derived.h"
#include "compiler.h"
//forward declarations
class WorkerObject;
class WatcherObject;


class Project
{
    public:
        Project(){};

        FileIndex Index;
        void Initialise(int argc,char**argv);
        void Connect(WorkerObject * worker, WatcherObject * watcher);
        WorkerObject * Worker = nullptr;
        WatcherObject * Watcher = nullptr;
        Metadata Info;
        CompilerObject Compiler;
        SettingsObject Settings;
        DerivedSettings Values;

        bool Synchronise(bool save=true);
        static void Prompt();
        void Clean();
    private:
        SettingsObject CachedSettings;
        void SetMetadata();

};

