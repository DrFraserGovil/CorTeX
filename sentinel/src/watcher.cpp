#include "watcher.h"

namespace fs = std::filesystem;

SystemWatcher::SystemWatcher(std::filesystem::path root): Root(root)
 {
    watcherID= inotify_init();
    if (watcherID < 0)
    {
        LOG(ERROR) << "Could not establish inotify process\nReason: " << std::strerror(errno);
        exit(1);
    }

    //find directories to watch
    PopulateDirectories("");
    Active = true;
}

SystemWatcher::~SystemWatcher()
{
    Stop();
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
        int rootWd = inotify_add_watch(watcherID, root.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
        watchMap.Insert(rootWd,target);
        newDirectories.insert(target);
    }

    LOG(DEBUG) << "Beggining scan " << root;

    // Scan through directories, adding them to the watchlist.
    for (auto   it = fsdir(root,ec); it != fsdir(); ++it) 
    {
        if (ec || !it->is_directory()) continue;
        
        auto p = it->path();
        auto relp = fs::relative(p,Root);
        //We can specify directories to ignore - prevents monitoring git/build.
        bool isIgnored = glob(relp.string(),Settings.Files.IgnoredPatterns);
        if (isIgnored)
        {
            it.disable_recursion_pending();
            LOG(DEBUG) << "Ignoring " << relp << " and all descendents";
            continue;
        }
        else
        {
            LOG(DEBUG) << "Watching " << relp;
            int wd = inotify_add_watch(watcherID, p.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
            watchMap.Insert(wd,relp);
            newDirectories.insert(relp);
        }
    }  
    LOG(DEBUG) << "population complete";

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


std::condition_variable & SystemWatcher::Start()
{
    Stop();
    lastEventTime = std::chrono::steady_clock::now();
    AsyncThread = std::thread(&SystemWatcher::ListenLoop,this);
    Active = true;
    return waitVariable;
}
void SystemWatcher::Stop()
{
    // Wake the watchdog up to see that it's hometime
    {
        std::lock_guard<std::mutex> lock(bucketMutex);
        Active = false;
    }
    if (AsyncThread.joinable())
    {
        AsyncThread.join();
    }
}

void SystemWatcher::ListenLoop()
{
    char buffer[4096];
    while (Active)
    {
        int length = read(watcherID, buffer, sizeof(buffer)); //blocks, so this thread is then quiet!
        if (length < 0) break;

        AddToBuffer(buffer, length);
    }
}

void SystemWatcher::AddToBuffer(char * buffer, int length)
{
    int i = 0;
    bool interrupt = false;
    std::set<FileReport> batch;
    while (i < length)
    {
        struct inotify_event* event = (struct inotify_event*)&buffer[i];
        if (event->len)
        {
            auto report = FileReport(Root/watchMap[event->wd],event);

            if (report.IsTerminationSequence)
            {
                LOG(DEBUG) << report.Path.stem() << " detected. Foamtex disabled once current batch complete. File will be deleted.";
                Active=false;
                std::filesystem::remove(report.Path);
                interrupt = true;
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
    if (interrupt || !batch.empty())
    {
        { ///extra scope makes sure the lock expires before the notify called
            std::lock_guard<std::mutex> lock(bucketMutex);
            lastEventTime = std::chrono::steady_clock::now();

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
            }
        }
        waitVariable.notify_one(); // Wake up the watchdog
    }
}


void SystemWatcher::NewWatchedDirectory(std::filesystem::path path)
{
    if (!glob(path.string(),Settings.Files.IgnoredPatterns))
    {
        LOG(DEBUG) << "A new directory was created (" << path << "), monitoring ";
        int wd = inotify_add_watch(watcherID, path.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
        // watchMap[wd] = path;
    }   
}

void SystemWatcher::DeleteWatchedDirectory(std::filesystem::path path)
{
}

FileReport::FileReport(fs::path path, inotify_event *event)
{
    Path = fs::relative(path / event->name,Settings.Files.TargetDirectory);
    Mask = event->mask;
    std::string stem = Path.stem();
    IsTerminationSequence = JSL::insensitiveEquals(stem, Settings.System.TerminationFileName);
}

bool FileReport::operator<(const FileReport &other) const
{
    return Path < other.Path;
}
