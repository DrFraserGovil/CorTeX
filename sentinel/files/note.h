#pragma once
#include "../constants.h"
#include <unordered_map>

class Note
{
    public:
        Note(fs::path path){
            LOG(INFO) << "Found " << path;
        };
    private:
    fs::path SourcePath;
    fs::path CompilePath;
    std::vector<std::string> Aliases;
    std::vector<std::weak_ptr<Note>> InboundLinks;
    
    std::unordered_map<std::string, std::weak_ptr<Note>> OutboundLinks;
    std::vector<std::string> OrphanedLinks;
};