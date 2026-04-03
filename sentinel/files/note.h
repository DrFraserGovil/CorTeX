#pragma once
#include "../constants.h"
#include <unordered_map>
#include <memory>

class Note
{
    public:
        Note(fs::path path,bool isError=false);

        static std::shared_ptr<Note> Create(fs::path path);
        bool IsError = false;
        friend class FileIndex;
    private:
        int UniqueID;
        fs::path SourcePath;
        fs::path CompilePath;
        std::vector<std::string> Aliases;
        std::vector<std::weak_ptr<Note>> InboundLinks;
        
        std::unordered_map<std::string, std::weak_ptr<Note>> OutboundLinks;
        std::vector<std::string> OrphanedLinks;
};

typedef std::weak_ptr<Note> NotePtr;
//SSoT for where note types are included
