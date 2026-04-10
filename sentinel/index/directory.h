#pragma once
#include <filesystem>
#include <set>
#include <vector>

#include "../note/note.h"
namespace fs = std::filesystem;



class Directory :public std::enable_shared_from_this<Directory>
{
    public:
        static std::shared_ptr<Directory> MakeFrom(fs::path path);
        
        PathObj Path;
        
       
        void Walk();
        
    
        //pointer shenanigans, so delete copy 
        Directory(const Directory&) = delete;
        Directory& operator=(const Directory&) = delete;
        

        //semi-private constructors so that MakeFrom is the only way to construct them, which ensures that it does the Walk()
        // A tiny struct only Directory can instantiate
        struct ConstructorKey { explicit ConstructorKey(int) {} };
        Directory(ConstructorKey key, fs::path path, std::weak_ptr<Directory> parent);
        
        std::set<std::shared_ptr<Directory>> Children;
        std::vector<std::weak_ptr<Note>> Notes;
        bool IsRoot;

        void Connect();

        std::weak_ptr<Directory> Find(std::vector<std::string_view> arr);

    private:
        
        Directory();
        
        int INotifyID = -1;
        std::weak_ptr<Directory> Parent;
        bool HasBeenDeleted;

        void ExistenceSweep();

        void NewDirectory(fs::path path);
        void NewNote(fs::path path);
};