#include "watcher.h"
#include "worker.h"

SystemWatcher::SystemWatcher(std::filesystem::path root, Worker & W): Root(root), Manager(W)
 {
    //initialise the inotify process
    watcherID= inotify_init();
    if (watcherID < 0)
    {
        LOG(ERROR) << "Could not establish inotify process\nReason: " << std::strerror(errno);
        exit(1);
    }

    //find directories to watch
    PopulateDirectories("");
}

SystemWatcher::~SystemWatcher()
{
    if (Active)   Stop();
}

bool is_prefix(const fs::path& parent, const fs::path& child) {
    auto p_it = parent.begin();
    auto c_it = child.begin();

    // Iterate through both paths part by part
    while (p_it != parent.end() && c_it != child.end()) {
        if (*p_it != *c_it) return false;
        ++p_it;
        ++c_it;
    }

    // If we reached the end of the parent path, it's a prefix
    return p_it == parent.end();
}
void SystemWatcher::RemoveWatches(fs::path root)
{
    // fs::path path =
    auto it = watchMap.LowerBound( fs::relative(root,Settings.Files.TargetDirectory));

    while (it != watchMap.rend() && is_prefix(root,it->first))
    {
        auto path = it->first;
        auto wd = watchMap[path];
        inotify_rm_watch(watcherID,wd);
        it = watchMap.Erase(it);
    }
}

std::set<fs::path> SystemWatcher::PopulateDirectories(std::filesystem::path target)
{
    RemoveWatches(target);
    LOG(DEBUG) << "Old watches removed. Currently watching " << watchMap.size() << " directories";
    fs::path root = Root/target;
    std::set<fs::path> newDirectories;

    std::error_code ec;
    using fsdir = fs::recursive_directory_iterator;
    //Watch the root by default, if it exists
    auto tmp = fsdir(root,ec);
    if (!ec)
    {
        WatchDirectory(target);
        newDirectories.insert(target);
    }


    // Scan through directories, adding them to the watchlist.
    for (auto   it = fsdir(root,ec); it != fsdir(); ++it) 
    {
        if (ec || !it->is_directory()) continue;
        
        auto p = it->path();
        auto rel_path = fs::relative(p,Root);

        bool isIgnored = WatchDirectory(rel_path);
        if (isIgnored)
        {
            it.disable_recursion_pending();
            continue;
        }
        else
        {
            newDirectories.insert(rel_path);
        }
    }  

    return newDirectories;
}

std::set<fs::path> SystemWatcher::GetWatchedDirs()
{
    std::set<fs::path> watchedDirs;
    for (auto dir: watchMap)
    {
        watchedDirs.insert(dir.second);
    }
    return watchedDirs;
}


void SystemWatcher::Start()
{
    if (Active) Stop();
    Active = true;
    Listener = std::thread(&SystemWatcher::ListenLoop,this);
}
void SystemWatcher::Stop()
{
    std::lock_guard<std::mutex> lock(Manager.JobLock);
    // Wake the watchdog up to see that it's hometime

    Active = false;
    auto terminate = (*watchMap.begin()).second / Settings.System.TerminationFileName; //ensure the file ends up in a directory that is being watched
    
    JSL::writeStringToFile(terminate,"",std::ios::out);
    if (Listener.joinable())
    {
        Listener.join();
    }
    fs::remove(terminate);
    Manager.Notify.notify_all();
    LOG(DEBUG) << "Watcher process terminated";
    
}

void SystemWatcher::ListenLoop()
{
    LOG(DEBUG) << "Watcher listen loop initialised";
    Manager.Initialised = true;
    Manager.Notify.notify_all();
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
            auto report = FileReport(Root/watchMap[event->wd],event);

            if (report.IsTerminationSequence)
            {
                Active=false;
                std::filesystem::remove(report.Path);
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
            LOG(INFO) << "Watcher reports change to " << report.Path;
        }
    }
    Manager.FileChange();
}


bool SystemWatcher::WatchDirectory(std::filesystem::path path)
{
    if (!glob(path.string(),Settings.Files.IgnoredPatterns))
    {
        LOG(DEBUG) << "Watching directory " << path;   
        auto fullpath = Root/path;
        int wd = inotify_add_watch(watcherID, fullpath.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
        watchMap.Insert(wd,path);
        return false;
    }
    else
    {
        return true;
    }
}

void SystemWatcher::DeleteWatchedDirectory(std::filesystem::path path)
{
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