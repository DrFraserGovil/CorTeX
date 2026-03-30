#pragma once
#include <map>
#include <set>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <sys/inotify.h>
#include "JSL/modules/Display/Log.h"
#include "utils.h"
#include "bidrectional_map.h"

#include "../constants.h"
#include "../settings/settings.hpp"
#include "report.h"
// #include "worker.h"
class Worker;

class SystemWatcher
{
    public:
    
        SystemWatcher(std::filesystem::path root,Worker & W);
        ~SystemWatcher();
        void Start();
        void Stop();
        
        std::set<fs::path> PopulateDirectories(std::filesystem::path root);
        void RemoveWatches(fs::path root);
        std::set<fs::path> GetWatchedDirs();
        bool Active=false;
        const fs::path Root;
        std::set<FileReport> GetTask();    
    private:
        std::map<fs::path,FileReport> dirtyFiles;
        Worker & Manager;
        int watcherID;
        BiAccessMap<int,fs::path> watchMap;

        void AddToBuffer(char * buffer, int length);

        std::mutex WatcherSync;
       
        bool WatchDirectory(std::filesystem::path path);
        
        void DeleteWatchedDirectory(std::filesystem::path path);
        
        std::thread Listener;

        void ListenLoop();
        
};