#pragma once
#include <memory>
#include <deque>
#include <map>
#include "../note/note.h"
#include "directory.h"
#include "aliases.h"
class FileIndex
{
    public:
        void Initialise();
        std::weak_ptr<Note> NewNote(fs::path path, std::weak_ptr<Directory> parent);
        void NotifyDirty(int id);
        std::shared_ptr<Directory> RootDir;
        std::weak_ptr<Note> GetNote(fs::path path);
        void Delete(std::weak_ptr<Note> note);

        friend class CompilerObject;
        bool IsDirty();
        std::weak_ptr<Note> GetLink(std::string_view key, fs::path requestingFile);
        void UpdateLinkNetwork(bool forceAll = false);
    private:
        int SequentialID;
        AliasList Aliases;
        std::map<int, std::shared_ptr<Note>> Registry;
        std::map<fs::path, int> PathRegistry;
        std::deque<int> DirtyFiles;
        std::weak_ptr<Note> Register(std::shared_ptr<Note> note);
};

