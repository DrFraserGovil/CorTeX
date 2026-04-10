#pragma once
#include <thread>
#include <poll.h>
#include <vector>
#include <functional>
#include <vector>
class WatcherObject
{
    public:
        WatcherObject();
        void Exit();

        void Start();
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

        int WatcherID;
        char buffer[4096];
};