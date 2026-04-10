#pragma once
#include <memory>
#include <filesystem>
#include "note_helpers.h"
#include "fileHeader.h"
#include "linkSet.h"

class Directory;
class Note
{
    public:
        PathObj Path;
        const int ID;
        bool IsDirty;
        bool HasBeenDeleted;
        void DiskCheck();
        Note(int id, std::filesystem::path path, std::weak_ptr<Directory> parent);
        FileHeader Header;
        void Build(std::string_view preamble,int Truncation);
        void Scan(bool saveToBuffer);
        friend class FileIndex;
        friend class CompilerObject;
    private:
        bool FailedToCompile = false;
        BufferObj Buffer;
        std::weak_ptr<Directory> Parent;
        void CheckLinks();
        LinkSet Links;
        int BodyStartLine;
        void Delete();
    // static std::shared_ptr<Note> Create(std::filesystem::path path, std::weak_ptr<Directory> parent);
};