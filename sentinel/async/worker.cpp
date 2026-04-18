#include "worker.h"
#include "../global.h"
#include "tasks/worker_functions.h"
void WorkerObject::ProcessInput()
{
    if (Cortex.Index.IsDirty())
    {
        LOG(DEBUG) << "Initial compilation sweep required";
        Cortex.Compiler.Run(false);
        Cortex.Prompt();
    }

    Active = true;
    while (Active)
    {
        std::unique_lock<std::mutex> lock(JobLock);
        Notify.wait(lock);
        std::swap(LocalJobs,Jobs);
        lock.unlock();

        while (LocalJobs.size() > 0)
        {
            ProcessHead();
        }
        LOG(DEBUG) << "Task queue empty, awaiting new tasks";
    }
}

void WorkerObject::AddTask(Task & newjob)
{
    if (newjob.Type != Instruction::None)
    {
        std::lock_guard<std::mutex> lock(JobLock);
        
        LOG(DEBUG) << "A new task of id " << (int)newjob.Type << " added.";
        Jobs.push(newjob);
        Notify.notify_one();
    }
}

void WorkerObject::AddTask(Instruction cmd)
{
    Task t(cmd);
    AddTask(t);
}


void WorkerObject::ProcessHead()
{
    auto job = LocalJobs.front();
    LocalJobs.pop();
    LOG(DEBUG) << JSL::Text::Colour(50,50,80) << "Processing job (type " << (int)job.Type <<")";
    Cascade=false;
    TotalCascade = false;
    if (Handlers.contains(job.Type))
    {
        Handlers[job.Type](job.TaskData);
    }
    else
    {
         LOG(WARN) << "Unimplemented instruction recieved";
    }
  
    //flag which is set if a command makes changes that need recompiling
    if (Cascade)
    {
        Instruction cmd = Instruction::IncrementalCompile;
        if (TotalCascade)
        {
            cmd = Instruction::Compile;
        }
        auto cascadeTask = Task(cmd);
        LocalJobs.push(cascadeTask);
    }



    //bit of manual hackery to get a reprompt
    if (!Cortex.Settings.System.Headless.Active && LocalJobs.size() == 0 && Active)
    {
        std::cout << JSL::Cursor::ClearLine <<  JSL::Text::Blue << ">> " << JSL::Text::Cyan << std::flush; 
    }
}


void WorkerObject::InstantTask(Task & job)
{
    LocalJobs.push(job);
    while (LocalJobs.size() > 0)
    {
        ProcessHead();
    }
}


void WorkerObject::SetHandlers()
{
    Handlers[Instruction::Shutdown] = [&](auto & data){
        Active = false;
    };

    Handlers[Instruction::SettingView] = [](auto & data)
    {
        ShowSetting(data);
    };

    Handlers[Instruction::SettingChange] = [&](auto & data){
        TotalCascade = ChangeSetting(data);
        Cascade |= TotalCascade;
    };
    Handlers[Instruction::SettingVectorAdd] = [&](auto & data){
        Cascade = ProcessVector<TryPush>(data);
    };
    Handlers[Instruction::SettingVectorRemove] = [&](auto & data){
        Cascade = ProcessVector<TryRemove>(data);
    };
    Handlers[Instruction::List] = [](auto & data)
    {
        directoryDisplay(data);
    };
    Handlers[Instruction::Clean] = [](auto & data)
    {
        Cortex.Clean();
    };
    Handlers[Instruction::Compile] = [](auto & data)
    {
        Cortex.Compiler.Run(true);
    };
    Handlers[Instruction::IncrementalCompile] = [](auto & data)
    {
        Cortex.Compiler.Run(false);
    };
    Handlers[Instruction::FileChange] = [&](auto & data)
    {
        Cascade = fileChange();
    };
    Handlers[Instruction::Reset] = [&](auto & data)
    {   
        TotalCascade = Reset(data);
        Cascade |= TotalCascade;
    };
    Handlers[Instruction::PauseSignal] = [&](auto & data)
    { 
        Cascade=Pause(data);  
    };

}

void WorkerObject::Prod()
{
    std::lock_guard<std::mutex> lock(JobLock);
    if (Jobs.size() > 0) Notify.notify_all();
}