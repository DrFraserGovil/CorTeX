#include "worker.h"

void Worker::WorkerLoop(SystemWatcher & watcher)
{
    // auto & blocker = watcher.Start();
    CurrentWatcher = &watcher;
    while (watcher.Active)
    {
        std::unique_lock<std::mutex> lock(JobLock);
        Notify.wait(lock);

        //even if wakeup is spurious, this is quick
        while (Jobs.size())
        {
            ProcessHead();   
        }
    }
}

void Worker::AddTask(Task newjob)
{
    std::lock_guard<std::mutex> lock(JobLock);
    Jobs.push(newjob);
    Notify.notify_one();
}

void Worker::ProcessHead()
{
    auto & job = Jobs.front();
    if (job.Type == Instruction::Shutdown)
    {
        Active = false;
        CurrentWatcher->Active = false;
    }
    Jobs.pop();
}