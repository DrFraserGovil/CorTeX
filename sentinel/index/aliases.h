#pragma once
#include <vector>
#include <string>
#include <memory>
#include "../note/note.h"

class AliasEntry
{
    public:
        std::string Key;
        std::vector<std::weak_ptr<Note>> Targets;
        AliasEntry(){};
        AliasEntry(std::string key, std::weak_ptr<Note> target);
        bool Remove(std::weak_ptr<Note> target);
        void Add(std::weak_ptr<Note> target);
        std::weak_ptr<Note> GetClosestLink(fs::path requestingFile);
        bool Contains(std::weak_ptr<Note> target);

};

class AliasList
{
    public:
        std::map<std::string, AliasEntry> Aliases;
        void Sync(std::weak_ptr<Note> target);
        void Remove(std::weak_ptr<Note> target);
        std::weak_ptr<Note> GetLink(std::string_view & key, fs::path requestingFile);
};