#pragma once

#include <mutex>
#include "tasks.h"
#include <queue>
#include "watcher.h"

class Worker
{
    public:
        void WorkerLoop(SystemWatcher & watcher);


        void AddTask(Task newjob);
        std::condition_variable Notify;
    private:
        bool Active;
        SystemWatcher * CurrentWatcher;
        std::queue<Task> Jobs;
        std::mutex JobLock;
        void ProcessHead();
};