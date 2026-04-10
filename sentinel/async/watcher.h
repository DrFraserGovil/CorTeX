#pragma once
#include <thread>
#include <poll.h>
#include <vector>
#include <functional>
#include <vector>
#include <memory>
#include <map>
class Directory;
class WatcherObject
{
    public:
        WatcherObject();
        void Exit();

        void Start();
        int WatchDir(std::weak_ptr<Directory> dir);
        // void Connect(Worker * worker);
    private:
        void Loop();

        void AddMenu();
        void AddFileWatch();
        void AddHeadlessWatch();
        std::atomic<bool> Running;
        std::thread Thread;

        std::vector<pollfd> Polls;
        std::vector<std::function<void()>> Callbacks;
        std::map<int,std::weak_ptr<Directory>> WatchMap;
        int WatcherID;
        char buffer[4096];
};