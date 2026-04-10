#pragma once
#include <condition_variable>
#include <mutex>
#include "tasks/tasks.h"
#include <queue>
#include "../settings/validate.h"
#include <map>
#include <functional>
class WorkerObject
{
    public:
        WorkerObject(){SetHandlers();};

        void ProcessInput();

        void AddTask(Task & newjob);
        void AddTask(Instruction cmd);
        void Prod();
        std::condition_variable Notify;
    private:
        bool Active;
        std::queue<Task> Jobs;
        std::queue<Task> LocalJobs;
        std::mutex JobLock;
        void ProcessHead();
        void ProcessFileChange();
        void SetHandlers();
        std::map<Instruction,std::function<void(std::vector<std::string> &)>> Handlers;
        bool Cascade;
        bool TotalCascade;
};