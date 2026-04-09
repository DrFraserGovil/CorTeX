#pragma once
#include <memory>
#include <filesystem>
#include "note_helpers.h"

class Note
{
    public:
        PathObj Path;
        const int ID;
        bool IsDirty;
        bool HasBeenDeleted;
        void DiskCheck();
        friend class FileIndex;
        Note(int id, std::filesystem::path path, std::weak_ptr<Directory> parent);
    private:
    // static std::shared_ptr<Note> Create(std::filesystem::path path, std::weak_ptr<Directory> parent);
};