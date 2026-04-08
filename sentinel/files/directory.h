#pragma once
#include <vector>
#include <memory>
#include "../constants.h"
#include "note.h"
#include <unordered_map>
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

        fs::path FullPath;
        fs::path OutputEquivalent;
        fs::path BuildEquivalent;
        std::unordered_map<fs::path, std::shared_ptr<Directory>> Children;
        std::unordered_map<fs::path, std::weak_ptr<Note>> Notes;
        void ConnectToINotify(SystemWatcher * Watcher);

        void Unwatch();
        void Delete();
        void Walk();
        void ReWalk();
        bool IsRoot = false;
        std::weak_ptr<Directory>  Find(std::vector<std::string_view> path);
        std::weak_ptr<Directory>  Find(fs::path path);
        void SetOutput();
        NotePtr NewNote(fs::path path); 
    private:
        int INotifyID;
        void NewEntity(fs::directory_iterator path,bool connect=false);
        std::weak_ptr<Directory> Parent;
        SystemWatcher * Watcher;
};

typedef std::shared_ptr<Directory> DirectoryPtr;