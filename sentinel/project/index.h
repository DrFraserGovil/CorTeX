#pragma once
#include <memory>
#include <map>
#include "../files/note.h"
#include "../files/directory.h"

class LinkResolver
{
    public:
        LinkResolver(){};
        void Add(std::weak_ptr<Note> note){Targets.push_back(note);}
        void Remove(std::weak_ptr<Note> note){};
    private:
        std::vector<NotePtr> Targets;
};


class FileIndex
{
    public:
        void Initialise();
    
        void DeleteFile(std::weak_ptr<Note> file);
        void Register(std::shared_ptr<Note> newNote);
        int GetID();
        std::weak_ptr<Directory> GetStructure();
        void UnwatchAll();
        void NotifyDirty(int id);
        void Compile(bool forceAll =false);
        void FindFile(fs::path path);
        bool IsDirty();
    private:
        int SequentialID=0;
        std::shared_ptr<Directory> Structure;
        std::map<int,std::shared_ptr<Note>> Registry;
        std::map<std::string,LinkResolver> Aliases;
        std::deque<int> DirtyFiles;
};

extern FileIndex MasterIndex;