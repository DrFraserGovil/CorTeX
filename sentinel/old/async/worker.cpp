#include "worker.h"
#include "watcher.h"
#include "../project/index.h"

void Worker::WorkerLoop(SystemWatcher & watcher)
{
    if (MasterIndex.IsDirty())
    {
        LOG(DEBUG) << "Initial compilation sweep required";
        MasterIndex.Compile();
    }
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
        // LOG(DEBUG) << "Worker has woken. " << LocalJobs.size() << " job(s) in queue";
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
        if (iNotifyCooldown) //the worker is already about to 
        {
            return;
        }
        Jobs.push(Task(Instruction::FileChange)); //pushes the task to the WorkerLoop thread to run the Process File change
        iNotifyCooldown = true;
        CooldownStart = std::chrono::steady_clock::now();
        Notify.notify_one();
    }
};


void Worker::AddTask(Task newjob)
{
    LOG(DEBUG) << "A new task of id " << (int)newjob.Type << " added " << Jobs.size();
    std::lock_guard<std::mutex> lock(JobLock);
    Jobs.push(newjob);
    Notify.notify_one();
    LOG(DEBUG) << "\tJob added to queue at position " << Jobs.size();
}


void Worker::ProcessFileChange()
{
    LOG(DEBUG) << "Confirming directory integrity";
    MasterIndex.GetStructure().lock()->ExistanceSweep(); //perform a full resweep of the structure to ensure that everything still works fine

    LOG(DEBUG) << "Directory integrity confirmed";

    //wait on this thread, allow the watcher thread to collate info
    auto elapsed = std::chrono::steady_clock::now() - CooldownStart;
    auto sleepTime = std::chrono::milliseconds(Cortex.Settings.System.DispatchDelay) - elapsed;
    if (sleepTime > std::chrono::milliseconds(0))
    {
        LOG(DEBUG) << JSL::Text::Colour(70,40,40) << "Debouncing";
        std::this_thread::sleep_for(sleepTime);
    }

    auto reports = CurrentWatcher->GetTask();
    bool dirSweep = false;
    for (auto report : reports)
    {
        if ((report.Mask & IN_ISDIR) && !dirSweep)
        {
            MasterIndex.GetStructure().lock()->ReWalk();
            dirSweep = true; // ensure we only do this once per report - its a clean slate wipe
        }
        else
        {
            MasterIndex.FindFile(report.Path);
        }

        
    }
    {
        std::lock_guard<std::mutex> lock(JobLock);
        iNotifyCooldown = false;
    }
    LOG(DEBUG) << "Beginning recompilation sweep";
    MasterIndex.Compile();
}

std::pair<bool,JSL::ParameterDescription> CheckParameterData(std::vector<std::string> & data)
{
    std::pair<bool,JSL::ParameterDescription> out(false,JSL::ParameterDescription());
    if (data.size() == 0)
    {
        LOG(WARN) << "Please specify a parameter to modifiy";
        return out;
    }

   
    auto counts = Cortex.Settings.GetDescription(data[0]);
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

bool ProcessParameterSet(std::vector<std::string> & data)
{
    auto [valid,description] = CheckParameterData(data);
    if (!valid)
    {
        return false;
    }
    data[0] = description.Key; //swap in the key in case the user gave the parameter name
    LOG(DEBUG) << "Processing change to " << description.Key;
    try
    {
        Cortex.Settings.ParseLine(data);
       
        bool requiresRecompile = ValidateSettings();
        LOG(DEBUG) << "Validate " << requiresRecompile;
        fs::path settings = (fs::path)Cortex.Settings.Files.TargetDirectory / settingLocation;
        Cortex.Settings.SaveConfig(settings);
        return requiresRecompile;
    }
    catch (...)
    {
        LOG(WARN) << "An error was encountered whilst parsing your argument.";
        return false;
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
bool ProcessVector(std::vector<std::string> & data)
{
    auto [valid,description] = CheckParameterData(data);
    if (!valid)
    {
        return false;
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

        bool requiresRecompile = ValidateSettings();
        fs::path settings = (fs::path)Cortex.Settings.Files.TargetDirectory / settingLocation;
        Cortex.Settings.SaveConfig(settings);
        return requiresRecompile;
    }
    catch (...)
    {
        LOG(WARN) << "An error was encountered whilst parsing your argument.";
        return false;
    }
}


void AttemptCompilation(std::vector<std::string> & data)
{
    if (data.size() == 0)
    {
        MasterIndex.Compile(true);
    }
}

void Worker::ProcessHead()
{
    auto & job = LocalJobs.front();
    LOG(DEBUG) << JSL::Text::Colour(50,50,80) << "Processing job (type " << (int)job.Type <<")";
    bool cascade=false;
    switch(job.Type)
    {
        
        case Instruction::Shutdown:
            Active = false;
            break;
        case Instruction::CompileRequest:
            AttemptCompilation(job.TaskData);
            break;
        case Instruction::FileChange:
            ProcessFileChange();
            break;
        case Instruction::ParameterUpdate:
            cascade = ProcessParameterSet(job.TaskData);
            break;
        case Instruction::VectorAdd:
             cascade =ProcessVector<TryPush>(job.TaskData);
            break;
        case Instruction::VectorRemove:
             cascade =ProcessVector<TryRemove>(job.TaskData);
            break;
        case Instruction::Clean:
            MasterIndex.CleanOutput();
            break;
        default:
            LOG(WARN) << "Unimplemented instruction recieved";
            break;
    }
    LOG(DEBUG) << "Cascade: " << cascade;
    if (cascade)
    {
        LocalJobs.push(Task(Instruction::CompileRequest));
    }
    LOG(DEBUG) << "Task " << (int)job.Type << " complete ";
    LocalJobs.pop();

    if (JSL::Log::Config.Level == DEBUG && LocalJobs.size() == 0)
    {
        std::cout << JSL::Cursor::ClearLine <<  JSL::Text::Blue << ">> " << JSL::Text::Cyan << std::flush; //bit of manual hackery to get a reprompt
    }
}
