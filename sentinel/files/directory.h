#pragma once
#include <vector>
#include <memory>
#include "../constants.h"
#include "note.h"

class SystemWatcher; //forward declaration

 class Directory :public std::enable_shared_from_this<Directory>
{
    
    public:
        Directory(){};
        Directory(fs::path path);
        Directory(fs::path path,std::weak_ptr<Directory> parent);
        //pointer shenanigans, so delete copy 
        Directory(const Directory&) = delete;
        Directory& operator=(const Directory&) = delete;

        static std::shared_ptr<Directory> GetRoot();
        fs::path FullPath;
        std::unordered_map<fs::path, std::shared_ptr<Directory>> Children;
        std::unordered_map<fs::path, std::shared_ptr<Note>> Notes;
        void ConnectToINotify(SystemWatcher * Watcher);

        void Unwatch();

    protected:
        void Walk();
    private:
        int INotifyID;
        std::weak_ptr<Directory> Parent;
        SystemWatcher * Watcher;
};

typedef std::shared_ptr<Directory> DirectoryPtr;