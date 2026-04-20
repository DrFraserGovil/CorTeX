#pragma once

#include "metadata.h"
#include "../index/fileindex.h"
#include "../settings/derived.h"
#include "compiler.h"
#include "../async/headless.h"
#include "../resources/colours.h"
//forward declarations
class WorkerObject;
class WatcherObject;


enum Mode
{
    Interactive,
    SingleCommand,
};
class Project
{
    public:
        Project(){};

        FileIndex Index;
        Mode Initialise(int argc,char**argv);

        void BeginInterface();
        void SingleCommand();

        void Connect(WorkerObject * worker, WatcherObject * watcher);
        WorkerObject * Worker = nullptr;
        WatcherObject * Watcher = nullptr;
        Metadata Info;
        CompilerObject Compiler;
        SettingsObject Settings;
        DerivedSettings Values;
        ColourHolder Colours;
        HeadlessInterface Antenna;

        bool Synchronise(bool save=true);
        static void Prompt();
        void Clean();

        void Shutdown();
    private:
        SettingsObject CachedSettings;
        void SetMetadata();
        std::string CachedCommand;
    
};

