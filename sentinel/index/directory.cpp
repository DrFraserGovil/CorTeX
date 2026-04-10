#include "directory.h"
#include "../global.h"
#include "glob.h"
#include <sys/inotify.h>
#include "../async/watcher.h"
std::shared_ptr<Directory> Directory::MakeFrom(fs::path path)
{
    auto out = std::make_shared<Directory>(ConstructorKey{0}, path,std::weak_ptr<Directory>{});
    out->Walk();
    return out;
}


Directory::Directory(){}

Directory::Directory(ConstructorKey key, fs::path path, std::weak_ptr<Directory> parent) : Parent(parent)
{
    IsRoot = (Parent.use_count() == 0); // check if default constructed

    INotifyID = -1;

    Path = PathObj(path);

    ExistenceSweep();
}

void Directory::ExistenceSweep()
{
    auto tryCreate = [&](fs::path path){
        if(!fs::exists(path)) fs::create_directories(path);
    };
    HasBeenDeleted = false;
    if (fs::exists(Path.Source))
    {
        HasBeenDeleted = true;
        return;
    }

    tryCreate(Path.Compile);
    tryCreate(Path.Build);

    for (auto & child : Children)
    {
        child->ExistenceSweep();
    }

    for (auto & note : Notes)
    {
        auto n = note.lock();
        n->DiskCheck();
        if (n->IsDirty)
        {
            Cortex.Index.NotifyDirty(n->ID);
        }
    }
}

template<class T>
std::weak_ptr<T> find(std::vector<std::weak_ptr<T>> & list, fs::path target)
{

    for (auto & el : list)
    {
        auto & path = el.lock()->Path.Source;
        if (target == path)
        {
            return el;
        }
    }
    return std::weak_ptr<T>{};
}

void Directory::Walk()
{

    //create a list of the original children (this is empty if first walk)
    std::vector<std::weak_ptr<Directory>> originalChildren;
    for (auto child : Children) originalChildren.push_back(child);
    std::vector<std::weak_ptr<Note>> originalNotes;
    for (auto child : Notes) originalNotes.push_back(child);

    std::error_code ec;
    using fsdir = fs::directory_iterator;

    for (auto it = fsdir(Path.Source,ec); it !=fsdir(); ++it)
    {
        if (ec) continue;

        auto child = it->path();
        
        //CASE: DIRECTORY
        if (fs::is_directory(child))
        {
            auto directory = find(originalChildren,child);

            if (directory.use_count()==0)
            {
                //(maybe) create new directory
                NewDirectory(child);
            }
            else
            {
                //this is the case of a directory we already know
                //walk it recursively
                directory.lock()->Walk();
            }


        }

        //CASE: NOTE FILE
        if (fs::is_regular_file(child))
        {
            auto file = find(originalNotes,child);
            if (file.use_count()==0)
            {
                NewNote(child);
            }
            //we do nothing if file already exists: we already have a record, and can't recurse into it!
        }
    }
}

void Directory::Connect()
{
    INotifyID = Cortex.Watcher->WatchDir(shared_from_this());

    for (auto & child : Children)
    {
        if (child->INotifyID == -1)
        {
            child->Connect();
        }
    }
    
}

void Directory::NewDirectory(fs::path path)
{
    bool ignored = glob(path,Cortex.Settings.Files.IgnoredPatterns);
    if (!ignored)
    {
        auto dir = std::make_shared<Directory>(ConstructorKey(0),path,shared_from_this());
        dir->Walk();
        if (Cortex.Watcher) //if the ptr is not null
        {
            dir->Connect();
        }
        Children.insert(dir);
    }
}

void Directory::NewNote(fs::path path)
{
    bool ignored = glob(path,Cortex.Settings.Files.IgnoredPatterns);
    if (!ignored)
    {
        bool isWatched = glob(path,Cortex.Settings.Files.WatchedPatterns);
        if (isWatched)
        {
            auto note = Cortex.Index.NewNote(path,shared_from_this());
            Notes.push_back(note);
        }
    }
}


std::weak_ptr<Directory> Directory::Find(std::vector<std::string_view> arr)
{

    std::string target = (std::string)arr[0];
    arr.erase(arr.begin());

    for (auto & child : Children)
    {
        // auto dir = child.second;
        std::string test = child->Path.Source.filename().string();
        if (test ==target)
        {
            if (arr.size() == 0)
            {
                return child;
            }
            else
            {
                return child->Find(arr);
            }
        }
    }

    LOG(WARN) << "Could not resolve '" << target <<"'. Best match is:";
    return shared_from_this();
    
}