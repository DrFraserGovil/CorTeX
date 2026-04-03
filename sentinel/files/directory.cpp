#include "directory.h"
#include "../settings/settings.hpp"
#include "glob.h"
#include "../async/watcher.h" //overrides the forward declaration

Directory::Directory(fs::path path)
{
    Parent.reset();
    FullPath = path;
    INotifyID = -1;
}
Directory::Directory(fs::path path,std::weak_ptr<Directory> parent) : Directory(path)
{
    Parent = parent;
}

void Directory::Walk()
{
    std::error_code ec;
    using fsdir = fs::directory_iterator;

    //now search for children
    for (auto   it = fsdir(FullPath,ec); it != fsdir(); ++it) 
    {
        if (ec ) continue;

        auto child = it->path();
        //ignore pattern specifies patterns in both directories and files which should be ignored
        bool ignored = glob(child,Settings.Files.IgnoredPatterns);
        if (!ignored)
        {
            if (it->is_directory())
            {
                //all non-ignored directories are traversed
                auto dirChild = std::make_shared<Directory>(child,shared_from_this());
                dirChild->Walk();
                Children[child] = (dirChild);
                continue;
            }
            if (it->is_regular_file())
            {
                //for files, we only want those which match our watch pattern
                if (glob(child,Settings.Files.WatchedPatterns))
                {
                    auto noteChild = Note::Create(child);
                    Notes[child] = noteChild;
                }
            }
        }
    }
}

DirectoryPtr Directory::GetRoot()
{
    auto root = std::make_shared<Directory>(Settings.Files.TargetDirectory);
    root->Walk();
    return root;
}

void Directory::ConnectToINotify(SystemWatcher * watcher)
{
    LOG(DEBUG) << "Initialising connection to " << FullPath;
    Watcher = watcher; //so they can delete themselves from the list if needs be
    INotifyID = inotify_add_watch(watcher->watcherID,FullPath.c_str(),IN_MODIFY | IN_CREATE | IN_DELETE);
    Watcher->WatchMap[INotifyID] = shared_from_this(); // a shared pointer to the present object
    //now recursively connect the children
    for (auto & child : Children)
    {
        child.second->ConnectToINotify(watcher);
    }


}

void Directory::Unwatch()
{
    for (auto & child : Children)
    {
        child.second->Unwatch();
    }
    inotify_rm_watch(Watcher->watcherID,INotifyID);
    Watcher->WatchMap.erase(INotifyID);
    
    Children.clear();
}