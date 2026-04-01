#include "worker.h"
#include "watcher.h"

void Worker::WorkerLoop(SystemWatcher & watcher)
{
    watcher.Start();
    LOG(DEBUG) << "Worker process initialised";
    CurrentWatcher = &watcher;
    {
        std::unique_lock<std::mutex> lock(JobLock);
        Notify.wait(lock);
    }
    Active = true;
    while (Active)
    {
        std::unique_lock<std::mutex> lock(JobLock);
        Notify.wait(lock);
        std::swap(LocalJobs,Jobs);
        LOG(DEBUG) << "Worker has woken. " << LocalJobs.size() << " jobs in queue";
        lock.unlock();
        //even if wakeup is spurious, this is quick
        while (LocalJobs.size() > 0)
        {
            ProcessHead();   
        }
    }
    watcher.Stop(); 
    LOG(DEBUG) << "Worker process terminated";
}

void Worker::WaitForInitialisation()
{
    std::unique_lock<std::mutex> lock(JobLock);
    while (!Initialised)
    {
        Notify.wait(lock);
    }
}

void Worker::FileChange() //function called by the watcher thread (async with the WorkerLoop)
{
    { 
        std::lock_guard<std::mutex> lock(JobLock);
        LOG(DEBUG) << "Pushing file change to worker thread";
        if (iNotifyCooldown) //the worker is already about to 
        {
            return;
        }
        Jobs.push(Task::FileChange()); //pushes the task to the WorkerLoop thread to run the Process File change
        iNotifyCooldown = true;
        CooldownStart = std::chrono::steady_clock::now();
        Notify.notify_one();
    }
};


void Worker::AddTask(Task newjob)
{
    std::lock_guard<std::mutex> lock(JobLock);
    Jobs.push(newjob);
    Notify.notify_one();
}


void Worker::ProcessFileChange()
{
    //wait on this thread, allow the watcher thread to collate info
    auto elapsed = std::chrono::steady_clock::now() - CooldownStart;
    auto sleepTime = std::chrono::milliseconds(Settings.System.DispatchDelay) - elapsed;
    if (sleepTime > std::chrono::milliseconds(0))
    {
        std::this_thread::sleep_for(sleepTime);
    }

    auto files = CurrentWatcher->GetTask();

    for (auto file : files)
    {
        LOG(INFO) << "Processing change to " << file.Path;
    }
    {
        std::lock_guard<std::mutex> lock(JobLock);
        iNotifyCooldown = false;
    }
}

std::pair<bool,JSL::ParameterDescription> CheckParameterData(std::vector<std::string> & data)
{
    std::pair<bool,JSL::ParameterDescription> out(false,JSL::ParameterDescription());
    if (data.size() == 0)
    {
        LOG(WARN) << "Please specify a parameter to modifiy";
        return out;
    }

   
    auto counts = Settings.GetDescription(data[0]);
    if (counts.size() == 0)
    {
        LOG(WARN) << "No parameter found matching key '" << data[0] << "'";
        return out;
    }
    if (counts.size() > 1)
    {
        LOG(WARN) << "Multiple parameters found matching that search:";
        for (auto count : counts)
        {
            LOG(WARN) << "  " << count.Name << " (key: " << count.Key << ")";
        }
        LOG(WARN) << "Please use a unique identifier";
        return out;
    }

    const std::vector<std::string> forbiddenValues = {"i","headless","h","config","config-delimiter"};
    if (std::find(forbiddenValues.begin(), forbiddenValues.end(),counts[0].Key)!=forbiddenValues.end())
    {
        LOG(WARN) << "Cannot mutate parameter '" << counts[0].Name << "' (-" <<counts[0].Key << ") at runtime. \nThis value can only be modified by the CLI";
        return out;
    }

    out.first = true;
    out.second = counts[0];
    return out;
}

void Worker::ProcessParameterSet(std::vector<std::string> & data)
{
    auto [valid,description] = CheckParameterData(data);
    if (!valid)
    {
        return;
    }
    data[0] = description.Key; //swap in the key in case the user gave the parameter name
    try
    {
        Settings.ParseLine(data);
       
        ValidateSettings();
        fs::path settings = (fs::path)Settings.Files.TargetDirectory / settingLocation;
        Settings.SaveConfig(settings);
    }
    catch (...)
    {
        LOG(WARN) << "An error was encountered whilst parsing your argument.";
    }
}

struct TryPush
{
    template<typename T>
     static void Run(JSL::ParameterDescription& desc, const std::string& value,bool & runningSuccess)
    {
        if (runningSuccess) return;
        if (desc.Type == typeid(std::vector<T>))
        {
            auto* vec = static_cast<std::vector<T>*>(desc.RiskyPointer);

            vec->push_back(JSL::ParseTo<T>(value));

            runningSuccess = true;
        }
    }
};

struct TryRemove
{
    template<typename T>
    static void Run(JSL::ParameterDescription& desc, const std::string& value,bool & runningSuccess)
    {
        if (runningSuccess) return;
        if (desc.Type == typeid(std::vector<T>))
        {
            auto* vec = static_cast<std::vector<T>*>(desc.RiskyPointer);

            T target = JSL::ParseTo<T>(value);
            auto it = std::find(vec->begin(),vec->end(),target);
            if (it==vec->end())
            {
                LOG(ERROR) << target << " is not in target. Cannot remove";
            }

            vec->erase(it);
            runningSuccess = true;
        }
    }
};



template<class Functor>
void ProcessVector(std::vector<std::string> & data)
{
    auto [valid,description] = CheckParameterData(data);
    if (!valid)
    {
        return;
    }
    std::ostringstream s(data[1]);
    for (size_t i = 2; i < data.size(); ++i)
    {
        s << " " << data[i]; 
    }
    std::string parser = s.str();
    
    try
    {
        bool hasSucceeded = false;

        Functor::template Run<int>(description,parser,hasSucceeded);
        Functor::template Run<double>(description,parser,hasSucceeded);
        Functor::template Run<std::string>(description,parser,hasSucceeded);
    
        if (!hasSucceeded)
        {
            LOG(WARN) << "Vector actions are not supported for objects of type '" << description.TypeString << "'";
        }

        ValidateSettings();
        fs::path settings = (fs::path)Settings.Files.TargetDirectory / settingLocation;
        Settings.SaveConfig(settings);
    }
    catch (...)
    {
        LOG(WARN) << "An error was encountered whilst parsing your argument.";
    }
}




void Worker::ProcessHead()
{
    auto & job = LocalJobs.front();
    switch(job.Type)
    {
        case Instruction::Shutdown:
            Active = false;
            break;
        case Instruction::FileChange:
            ProcessFileChange();
            break;
        case Instruction::ParameterUpdate:
            ProcessParameterSet(job.TaskData);
            break;
        case Instruction::VectorAdd:
            ProcessVector<TryPush>(job.TaskData);
            break;
        case Instruction::VectorRemove:
            ProcessVector<TryRemove>(job.TaskData);
            break;
        default:
            LOG(WARN) << "Unimplemented instruction recieved";
    }
    
    LocalJobs.pop();
}
