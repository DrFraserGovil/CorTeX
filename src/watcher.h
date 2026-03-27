#pragma once
#include "globalSettings.h"
#include <map>
#include <set>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <sys/inotify.h>
#include "JSL/modules/Display/Log.h"
#include "utils.h"
#include "network.h"

#include "bidrectional_map.h"
struct FileReport
{
    fs::path Path;
    uint32_t Mask;
    bool IsTerminationSequence;

    FileReport( fs::path path, inotify_event * event);
    bool operator<(const FileReport& other) const;
};




class SystemWatcher
{
    public:
    
        SystemWatcher(std::filesystem::path root);
        ~SystemWatcher();
        std::condition_variable & Start();
        void Stop();
        
        std::set<fs::path> PopulateDirectories(std::filesystem::path root);
        void RemoveWatches(fs::path root);
        std::set<fs::path> GetWatchedDirs();
        bool Active;
        std::mutex bucketMutex;
        std::map<fs::path,FileReport> dirtyFiles;
        std::chrono::steady_clock::time_point lastEventTime;
        const fs::path Root;
    private:
        WikiNetwork Network;
        int watcherID;
        BiAccessMap<int,fs::path> watchMap;
        std::condition_variable waitVariable;

        void AddToBuffer(char * buffer, int length);
        
        void ProcessBuffer(const std::set<std::filesystem::path>  & batch)
        {
            LOG(DEBUG) << "Processing batch of size " << batch.size();         
        }
       
        void NewWatchedDirectory(std::filesystem::path path);
        
        void DeleteWatchedDirectory(std::filesystem::path path);
        
        std::thread Listener;

        void ListenLoop();
        
};