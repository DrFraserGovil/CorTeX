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

    SetOutput();
}
Directory::Directory(fs::path path,std::weak_ptr<Directory> parent) : Directory(path)
{
    Parent = parent;
}

void Directory::SetOutput()
{
    auto Root = (fs::path)Cortex.Settings.Files.TargetDirectory;
    auto relpath = fs::relative(FullPath,Cortex.Settings.Files.TargetDirectory);
    OutputEquivalent = Root/Cortex.Settings.Files.OutputDirectory /relpath;
    BuildEquivalent = Root/Cortex.Settings.Files.BuildDirectory /relpath;
   
    ExistanceSweep();
}

void Directory::ExistanceSweep()
{
    if (!fs::exists(OutputEquivalent))
    {
        fs::create_directories(OutputEquivalent);
    }
    if (!fs::exists(BuildEquivalent))
    {
        fs::create_directories(BuildEquivalent);
    }
    
    for (auto & child : Children)
    {
        child.second->ExistanceSweep();
    }
    for (auto & note : Notes)
    {
        auto n = note.second.lock();
        note.second.lock()->DiskCheck();
        if (n->IsDirty())
        {
            MasterIndex.NotifyDirty(n->UniqueID);
        }
    }
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

NotePtr Directory::NewNote(fs::path child)
{
     auto noteChild = Note::Create(child,shared_from_this());
    Notes[child.filename()] = noteChild;
    return noteChild;
}

void Directory::NewEntity(fs::directory_iterator path,bool connect)
{
    auto child = path->path();
    //ignore pattern specifies patterns in both directories and files which should be ignored
    bool ignored = glob(child,Cortex.Settings.Files.IgnoredPatterns);
    if (!ignored)
    {
        if (path->is_directory())
        {
            //all non-ignored directories are traversed
            auto dirChild = std::make_shared<Directory>(child,shared_from_this());
            dirChild->Walk();
            if (connect)
            {
                dirChild->ConnectToINotify(Watcher);
            }
            Children[child] = (dirChild);
            return;
        }
        if (path->is_regular_file())
        {
            //for files, we only want those which match our watch pattern
            if (glob(child,Cortex.Settings.Files.WatchedPatterns))
            {
               NewNote(child);
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
            auto fileFind = Notes.find(child.filename());
            if (fileFind == Notes.end())
            {
                //anything that fails is either a new directory, or a file in a new directory that didn't otherwise trigger a newfile alert
                // (i.e. a file that existed before the directory was mv'd in)
                NewEntity(it,true); //this adds the new entity, and continues recursively
                
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
    for (auto & child : Children)
    {
        child.second->Delete();
        Children.erase(child.first);
    }
    if (fs::exists(OutputEquivalent))
    {
        fs::remove_all(OutputEquivalent);
    }
    if (fs::exists(BuildEquivalent))
    {
        fs::remove_all(BuildEquivalent);
    }
    Unwatch();
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
    
    LOG(DEBUG) << "Releasing inotify watch " << INotifyID << " on " << FullPath.filename().string();
    inotify_rm_watch(Watcher->watcherID,INotifyID);
    Watcher->WatchMap.erase(INotifyID);
    
    Children.clear();

    
}

std::weak_ptr<Directory> Directory::Find(std::vector<std::string_view> arr,bool softMatch)
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

    if (softMatch)
    {
        LOG(WARN) << "Could not resolve '" << target <<"'. Best match is:";
        return shared_from_this();
    }
    else{
        DirectoryPtr out;
        return out;
    }
}

std::weak_ptr<Directory> Directory::Find(fs::path path)
{
    if (path.empty())
    {
        return shared_from_this();
    }
    std::vector<std::string> array {path.begin(), path.end()};
    std::vector<std::string_view> arr {array.begin(), array.end()};
    return Find(arr,false);
}

void Directory::GatherOutputs(std::set<fs::path> & index)
{
    fs::path dir = (fs::path)Cortex.Settings.Files.TargetDirectory/ Cortex.Settings.Files.OutputDirectory;
    for (auto & note : Notes)
    {
        auto n = note.second.lock();
        index.insert(fs::relative(n->CompilePath,dir));
    }

    for (auto & child : Children)
    {
        auto c = child.second;
        c->GatherOutputs(index);
    }
}