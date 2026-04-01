#pragma once
// #include <map>
#include <set>
#include <mutex>
// #include <chrono>
// #include <condition_variable>
// #include <thread>
#include <sys/inotify.h>
#include "JSL/modules/Display/Log.h"
// #include "utils.h"
// #include "bidrectional_map.h"

// #include "../constants.h"
#include "../settings/settings.hpp"
#include "report.h"
// #include 
#include "worker.h"
#include "../files/directory.h"
class Worker;

class SystemWatcher
{
    public:
    
        SystemWatcher(DirectoryPtr root,Worker & W);
        ~SystemWatcher();


        void Start();
        void Stop();
        
        // // std::set<fs::path> PopulateDirectories(std::filesystem::path root);
        // void RemoveWatches(fs::path root);
        bool Active=false;
        std::set<FileReport> GetTask();    
        void AddWatches(DirectoryPtr head);
        friend class Directory;
    private:
        std::thread AsyncThread;
        DirectoryPtr  Root;
        Worker & Executor;
        int watcherID;
        std::map<int,std::weak_ptr<Directory>> WatchMap;
        std::map<fs::path,FileReport> dirtyFiles;
        std::mutex WatcherSync;

        void AddToBuffer(char * buffer, int length);
        // bool WatchDirectory(std::filesystem::path path);
        // void DeleteWatchedDirectory(std::filesystem::path path);
        void AsyncLoop();

        
};