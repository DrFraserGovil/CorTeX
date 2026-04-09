#pragma once
#include <condition_variable>
#include <mutex>
#include "tasks.h"
#include <queue>
#include "../settings/validate.h"
// #include "watcher.h"
class SystemWatcher;

class Worker
{
    public:
        void WorkerLoop(SystemWatcher & watcher);

        void WaitForInitialisation();
        void FileChange();
        void AddTask(Task newjob);
        std::condition_variable Notify;
        friend class SystemWatcher;
    private:
        std::chrono::steady_clock::time_point CooldownStart;
        bool Initialised = false;
        bool iNotifyCooldown =false;
        bool Active;
        SystemWatcher * CurrentWatcher;
        std::queue<Task> Jobs;
        std::queue<Task> LocalJobs;
        std::mutex JobLock;
        void ProcessHead();
        void ProcessFileChange();
};