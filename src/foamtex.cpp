#include "foamtex.h"

FoamTexArchive::FoamTexArchive(): Watcher(Settings.Files.TargetDirectory)
{
    auto dirs = Watcher.GetWatchedDirs();
    SourceCode.Initialise(dirs);

    Compiled.ForceReconstruct(SourceCode.ListFiles(),dirs);

    WatcherLoop();
}
void FoamTexArchive::WatcherLoop() {
    auto & blocker = Watcher.Start();

    
    while (Watcher.Active)
    {
        std::unique_lock<std::mutex> lock(Watcher.bucketMutex);
        
        //wait until the watcher signals. This is a thread-blocking call so not too CPU heavy until the signal is met
        blocker.wait(lock, [this] { return !Watcher.dirtyFiles.empty() || !Watcher.Active; });

        if (!Watcher.Active) break; 

        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastEvent = now - Watcher.lastEventTime;

        std::chrono::milliseconds delay{Settings.System.PollingDelay};
        if (timeSinceLastEvent < delay)
        {
            blocker.wait_until(lock,Watcher.lastEventTime+delay);
            continue; //we continue instead of falling through in case the thread was woken spuriously
        }

        std::map<fs::path,FileReport> batch;
        batch.swap(Watcher.dirtyFiles);
        lock.unlock();

        ProcessBuffer(batch);
        

    }

    Watcher.Stop();
}
void FoamTexArchive::ProcessBuffer(std::map<fs::path, FileReport> batch)
{
    LOG(WARN) << "New buffer update";
    for (auto path: batch)
    {
        auto & report= path.second;
        LOG(DEBUG) << "Detected change to " << report.Path;

        if (report.Mask & IN_ISDIR)
        {
            DirectoryUpdate(report);
        }
        else
        {
            if (SourceCode.Contains(report.Path))
            {  
                LOG(DEBUG) << "File exists within known repository";
            }
            else
            {
                LOG(DEBUG) << "This is a new file";
            }
        }
    }
}

#include <algorithm>


std::set<fs::path> Intersection(const std::set<fs::path> & setA, const std::set<fs::path> & setB)
{
    std::set<fs::path> result;
    std::set_intersection(setA.begin(), setA.end(), 
                      setB.begin(), setB.end(), 
                      std::inserter(result, result.begin()));
    return result;
}
std::set<fs::path> Difference(const std::set<fs::path> & setA, const std::set<fs::path> & setB)
{
    std::set<fs::path> result;
    std::set_difference(setA.begin(), setA.end(), 
                      setB.begin(), setB.end(), 
                      std::inserter(result, result.begin()));
    return result;
}

void FoamTexArchive::DirectoryUpdate(FileReport report) 
{
    //keep a copy of the old files so we can see what's changed
    auto oldFiles = SourceCode.ListFiles();
    std::set<fs::path> oldDirectories = Watcher.GetWatchedDirs();
    std::set<fs::path> newDirectories = Watcher.PopulateDirectories(report.Path);
    
    std::stringstream s;
    for (auto p : oldDirectories)
    {
        s << p << " ";
    }
    LOG(DEBUG) << s.str();

    auto unchanged = Intersection(oldDirectories,newDirectories);
    auto added = Difference(newDirectories,oldDirectories);
    auto deleted = Difference(oldDirectories,newDirectories);

    LOG(INFO) << "Final result " << unchanged.size() << " " << added.size() << " " << deleted.size();
    if (unchanged.size() > 0)
    {
        std::stringstream s;
        s << "The following directories were unchanged:";
        for (auto & p : unchanged){ s << "\n\t" << p;}
        LOG(DEBUG) << s.str();
    }
    if (added.size() > 0)
    {
        std::stringstream s;
        s << "The following directories were added:";
        for (auto & p : added){ s << "\n\t" << p;}
        LOG(DEBUG) << s.str();
    }
    if (deleted.size() > 0)
    {
        std::stringstream s;
        s << "The following directories were deleted:";
        for (auto & p : deleted){ s << "\n\t" << p;}
        LOG(DEBUG) << s.str();
    }
};