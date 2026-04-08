#include "watcher.h"
#include "worker.h"
#include "../files/glob.h"
#include "../project/index.h"
SystemWatcher::SystemWatcher(Worker & W): Root(MasterIndex.GetStructure()), Executor(W)
 {
    //inotify is a process which reports back when watched directories (and their contents)
    //undergo a change. It's a posix function so should exist everywhere (or have a compiler-flagged in alternative for windows)
    watcherID= inotify_init();
    if (watcherID < 0)
    {
        LOG(ERROR) << "Could not establish inotify process\nReason: " << std::strerror(errno);
        exit(1);
    }

    AddWatches(Root);
}


void SystemWatcher::AddWatches(DirectoryPtr head)
{
    head->ConnectToINotify(this);
}

SystemWatcher::~SystemWatcher()
{
    if (Active)   Stop();
    close(watcherID);
}

void SystemWatcher::Start()
{
    if (Active) Stop();
    Active = true;
    AsyncThread = std::thread(&SystemWatcher::AsyncLoop,this);
}
void SystemWatcher::Stop()
{
    std::lock_guard<std::mutex> lock(Executor.JobLock);
    // Wake the watchdog up to see that it's hometime

    Active = false;
    auto terminate = (*WatchMap.begin()).second.lock()->FullPath / Settings.System.TerminationFileName; //ensure the file ends up in a directory that is being watched
    
    JSL::writeStringToFile(terminate,"",std::ios::out);
    if (AsyncThread.joinable())
    {
        AsyncThread.join();
    }
    if (fs::exists(terminate)) fs::remove(terminate); //cleanup just in case
    Executor.Notify.notify_all();
    LOG(DEBUG) << "Watcher process terminated";
    
}

void SystemWatcher::AsyncLoop()
{
    LOG(DEBUG) << "Watcher listen loop initialised";
    Executor.Initialised = true;
    Executor.Notify.notify_all();
    char buffer[4096];
    while (Active)
    {
        int length = read(watcherID, buffer, sizeof(buffer)); //blocks, so this thread is then quiet!
        if (length < 0 || !Active) break;

        AddToBuffer(buffer, length);
    }
   
}

void SystemWatcher::AddToBuffer(char * buffer, int length)
{
    int i = 0;
    std::set<FileReport> batch;
    while (i < length)
    {
        struct inotify_event* event = (struct inotify_event*)&buffer[i];
        if (event->len)
        {
            auto report = FileReport(WatchMap[event->wd].lock()->FullPath,event);
            if (report.IsTerminationSequence)
            {
                std::filesystem::remove(report.Path);
                
                
                if (!Settings.System.Headless.Active)
                {
                    //have to hard slam on the breaks in this case: it's only reachable if the termination sequence is encountered whilst the menu is locked on std::cin.
                    LOG(ERROR) << "Termination sequence encountered whilst in interactive mode. \nSystem will experience a hard-shutdown"; 
                    exit(0);
                }
                Active=false;
                return;
            }
            else
            {            
                bool dirChange = report.Mask & IN_ISDIR;
                bool validFile = glob(report.Path,Settings.Files.WatchedPatterns);
                bool ignored = glob(report.Path,Settings.Files.IgnoredPatterns);
                if ((dirChange || validFile) && !ignored)
                {
                    
                    batch.insert(report);
                }
            }
        }
        i += sizeof(struct inotify_event) + event->len;
    }
    if (!batch.empty())
    {
        std::lock_guard<std::mutex> lock(WatcherSync);
        for (auto& report : batch)
        {
            // Check if we already have a report for this path
            auto [it, inserted] = dirtyFiles.try_emplace(report.Path, report);
            
            if (!inserted) 
            {
                // Path already existed! Absorption:
                // Merge the new mask into the existing one
                it->second.Mask |= report.Mask;
            }
            else
            {
                LOG(DEBUG) << "Watcher reports change to " << report.Path << ", code " << report.Mask;
            }
        }
    }
    Executor.FileChange();
}



std::set<FileReport> SystemWatcher::GetTask()
{
    std::set<FileReport> out;
    {
        std::lock_guard<std::mutex> lock(WatcherSync);
        for (auto & report : dirtyFiles)
        {
            out.insert(report.second);
        }
        dirtyFiles.clear();
    }
    return out;
}