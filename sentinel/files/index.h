#pragma once
#include <memory>
#include <map>
#include "note.h"

class LinkResolver
{
    public:
        LinkResolver(){};
        void Add(std::weak_ptr<Note> note){Targets.push_back(note);}
    private:
        std::vector<NotePtr> Targets;
};


class FileIndex
{
    public:
    
        void Register(std::shared_ptr<Note> newNote);
        int GetID();
    private:
        int SequentialID=0;
        std::map<int,std::shared_ptr<Note>> Registry;
        std::map<std::string,LinkResolver> Aliases;
};

extern FileIndex MasterIndex;