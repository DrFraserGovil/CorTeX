#include "directory.h"
#include "../settings/settings.hpp"
#include "glob.h"
#include "../async/watcher.h" //overrides the forward declaration
#include "../project/index.h"
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

        NewEntity(it);
    }
}

void Directory::NewEntity(fs::directory_iterator path)
{
    auto child = path->path();
    //ignore pattern specifies patterns in both directories and files which should be ignored
    bool ignored = glob(child,Settings.Files.IgnoredPatterns);
    if (!ignored)
    {
        if (path->is_directory())
        {
            //all non-ignored directories are traversed
            auto dirChild = std::make_shared<Directory>(child,shared_from_this());
            dirChild->Walk();
            Children[child] = (dirChild);
            return;
        }
        if (path->is_regular_file())
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

void Directory::ReWalk()
{
    //called when a directory-level change happens during a live watch. We perform a complete rescan and check for any differences
    // at the end, the system should have re-configured itself to the new layout with the minimal number of changes that require recompilation

    std::error_code ec;
    using fsdir = fs::directory_iterator;

    //now search for children
    std::set<fs::path> originalChildren;
    for (auto & child : Children){originalChildren.insert(child.first);}

    for (auto   it = fsdir(FullPath,ec); it != fsdir(); ++it) 
    {
        if (ec ) continue;

        auto child = it->path();

        auto find = Children.find(child);
        //if the directory is one we're already tracking, that's fine -- we mark it as found, and then keep descending
        if (find != Children.end())
        {
            originalChildren.erase(find->first);
            find->second->ReWalk();
        }
        else
        {
            //then test if its a file we already know about
            auto fileFind = Notes.find(child);
            if (fileFind == Notes.end())
            {
                //anything that fails is either a new directory, or a file in a new directory that didn't otherwise trigger a newfile alert
                // (i.e. a file that existed before the directory was mv'd in)
                NewEntity(it); //this adds the new entity, and continues recursively
            }

        }
    }

    //whatever is left no longer exists on disk: so delete it
    for (auto leftover : originalChildren)
    {
        Children[leftover]->Delete(); //signals the file to clean up after itself
        Children.erase(leftover); //deletes from memory
    }
}

void Directory::Delete()
{
    for (auto & note : Notes)
    {
        MasterIndex.DeleteFile(note.second);
    }
}

void Directory::ConnectToINotify(SystemWatcher * watcher)
{
    LOG(DEBUG) << "Initialising connection to " << FullPath;
    Watcher = watcher; //so they can delete themselves from the list if needs be
    INotifyID = inotify_add_watch(watcher->watcherID,FullPath.c_str(),IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE);
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

std::weak_ptr<Directory> Directory::Find(std::vector<std::string_view> arr)
{
    std::string target = (std::string)arr[0];
    arr.erase(arr.begin());

    for (auto & child : Children)
    {
        // auto dir = child.second;
        std::string test = child.second->FullPath.filename().string();
        if (test ==target)
        {
            if (arr.size() == 0)
            {
                return child.second;
            }
            else
            {
                return child.second->Find(arr);
            }
        }
    }

    LOG(WARN) << "Could not resolve '" << target <<"'. Best match is:";
    return shared_from_this();
}