#pragma once
#include <thread>
#include <poll.h>
#include <vector>
#include <functional>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include "report.h"

class Directory;
class WatcherObject
{
    public:
        WatcherObject();
        void Exit();

        void Start();
        int WatchDir(std::weak_ptr<Directory> dir);
        std::set<FileReport> GetReports();

        void AddHeadlessWatch();
    private:
        void Loop();

        void AddMenu();
        void AddFileWatch();
        std::atomic<bool> Running;
        std::thread Thread;

        std::vector<pollfd> Polls;
        std::vector<std::function<void()>> Callbacks;
        std::map<int,std::weak_ptr<Directory>> WatchMap;
        std::set<FileReport> Reports;
        std::mutex WatcherSync;
        int WatcherID;
        char buffer[4096];
};