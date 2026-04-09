#include "worker.h"
#include "../global.h"
#include "worker_functions.h"
void Worker::ProcessInput()
{
    // if (MasterIndex.IsDirty())
    // {
    //     LOG(DEBUG) << "Initial compilation sweep required";
    //     MasterIndex.Compile();
    // }

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
    }
}

void Worker::AddTask(Task & newjob)
{
    if (newjob.Type != Instruction::None)
    {
        LOG(DEBUG) << "A new task of id " << (int)newjob.Type << " added " << Jobs.size();
        std::lock_guard<std::mutex> lock(JobLock);
        Jobs.push(newjob);
        Notify.notify_one();
    }
}



void Worker::ProcessHead()
{
    auto & job = LocalJobs.front();
    LOG(DEBUG) << JSL::Text::Colour(50,50,80) << "Processing job (type " << (int)job.Type <<")";
    Cascade=false;

    if (Handlers.contains(job.Type))
    {
        Handlers[job.Type](job.TaskData);
    }
    else
    {
         LOG(WARN) << "Unimplemented instruction recieved";
    }
  
    //     // case Instruction::CompileRequest:
    //     //     AttemptCompilation(job.TaskData);
    //     //     break;
    //     // case Instruction::FileChange:
    //     //     ProcessFileChange();
    //     //     break;
  
    //     // case Instruction::Clean:
    //     //     MasterIndex.CleanOutput();
    //     //     break;
    //     default:
           
    //         break;
    // }

    //flag which is set if a command makes changes that need recompiling
    if (Cascade)
    {
        LocalJobs.push(Task(Instruction::Compile));
    }


    LocalJobs.pop();
    LOG(DEBUG) << "Task " << (int)job.Type << " complete" ;

    //bit of manual hackery to get a reprompt
    if (!Settings.System.Headless.Active && LocalJobs.size() == 0 && Active)
    {
        std::cout << JSL::Cursor::ClearLine <<  JSL::Text::Blue << ">> " << JSL::Text::Cyan << std::flush; 
    }
}

void Worker::SetHandlers()
{
    Handlers[Instruction::Shutdown] = [&](auto & data){
        Active = false;
    };

    Handlers[Instruction::SettingView] = [](auto & data)
    {
        ShowSetting(data);
    };

    Handlers[Instruction::SettingChange] = [&](auto & data){
        Cascade = ChangeSetting(data);
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
    // Handlers[Instruction::Clean] = [](auto & data)
    // {
    // }
    // Handlers[Instruction::Compile] = [](auto & data)
    // {
    // }
    // Handlers[Instruction::FileChange] = [](auto & data)
    // {
    // }
    // Handlers[Instruction::Reset] = [](auto & data)
    // {
    // }
}