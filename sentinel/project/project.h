#pragma once

#include "metadata.h"
#include "../index/fileindex.h"
#include "../settings/derived.h"

//forward declarations
class Worker;
class Watcher;


class Project
{
    public:
        Project(){};

        FileIndex Index;
        void Initialise(int argc,char**argv);
        void Connect(Worker * worker, Watcher * watcher);
        Worker * GlobalWorker;
        Watcher * GlobalWatcher;
        Metadata Info;
        SettingsObject Settings;
        DerivedSettings Values;

        bool Synchronise();
    private:
        SettingsObject CachedSettings;
        void SetMetadata();

};

