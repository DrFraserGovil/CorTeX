#pragma once
#include <memory>
#include <filesystem>
#include "note_helpers.h"
#include "fileHeader.h"
#include "linkSet.h"
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
        FileHeader Header;
        void Build(std::string_view preamble,int Truncation);
    private:
        void Scan(bool saveToBuffer);
        BufferObj Buffer;
        void CheckLinks();
        LinkSet Links;
        int BodyStartLine;
        void Delete();
    // static std::shared_ptr<Note> Create(std::filesystem::path path, std::weak_ptr<Directory> parent);
};